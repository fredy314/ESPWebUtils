/*
 * ESPWebUtils Library
 * Copyright (c) 2026 Fedir Vilhota <fredy31415@gmail.com>
 * * This software is released under the MIT License.
 * See the LICENSE file in the project root for full license information.
 */
#include "WiFiManager.h"
#include "secrets.h"
#include "esp_wifi.h"
#include <ESPmDNS.h>

WiFiManager::WiFiManager(
  WiFiNetwork* networks, 
  int networkCount,
  const char* hostname,
  IPAddress localIP,
  IPAddress gateway,
  IPAddress subnet,
  IPAddress primaryDNS,
  IPAddress secondaryDNS
) : _networks(networks),
    _networkCount(networkCount),
    _hostname(hostname),
    _localIP(localIP),
    _gateway(gateway),
    _subnet(subnet),
    _primaryDNS(primaryDNS),
    _secondaryDNS(secondaryDNS),
    _state(SCANNING),
    _stateStartTime(0),
    _lastScanAttempt(0),
    _currentNetworkIndex(0),
    _currentPowerIndex(0),
    _currentChannel(0),
    _isOpenNetwork(false),
    _isScanning(false) {
      
      memset(_currentBSSID, 0, 6);
      esp_wifi_set_ps(WIFI_PS_NONE);
      esp_wifi_set_country_code("UA", true);
}

WiFiManager::WiFiManager() : WiFiManager(
  wifiNetworks,
  wifiNetworkCount,
  WIFI_HOSTNAME,    // Використовуємо макроси напряму
  STATIC_IP,
  GATEWAY_IP,
  SUBNET_MASK,
  PRIMARY_DNS,
  SECONDARY_DNS
) {}

void WiFiManager::begin() {
  Serial.println("\n--- WiFiManager: Starting ---");
  Serial.printf("Hostname: %s\n", _hostname.c_str());
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  WiFi.setSleep(false);
  // Set hostname
  if (_hostname.length() == 0) {
    _hostname = generateHostname();
  }
  WiFi.setHostname(_hostname.c_str());
  _state = STARTING;
  _stateStartTime = millis();
}

void WiFiManager::tick() {
  switch (_state) {
    case STARTING:
      if (millis() - _stateStartTime > 200) {
        startScan();
      }
      break;
    case SCANNING:
      processScanResults();
      break;
    case DISCONNECTING:
      if (millis() - _stateStartTime > 500) {
        // Actual connection logic (moved from connectToNetwork)
        WiFi.mode(WIFI_STA);

        if (!_isOpenNetwork && _localIP != IPAddress(0, 0, 0, 0)) {
          if (_primaryDNS != IPAddress(0, 0, 0, 0)) {
            WiFi.config(_localIP, _gateway, _subnet, _primaryDNS, _secondaryDNS);
          } else {
            WiFi.config(_localIP, _gateway, _subnet);
          }
        } else {
          WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
        }
        WiFi.setTxPower(_powerSteps[_currentPowerIndex]);
        WiFi.begin(_currentSSID.c_str(), _pendingPassword.c_str(), _currentChannel, (_currentChannel > 0) ? _currentBSSID : nullptr);
        _state = _isOpenNetwork ? CONNECTING_OPEN : CONNECTING_KNOWN;
        _stateStartTime = millis();
      }
      break;
    case CONNECTING_KNOWN:
    case CONNECTING_OPEN:
      checkConnection();
      break;
    case CONNECTED:
      // Check if still connected
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi: Connection lost!");
        handleDisconnection();
      }
      break;
    case WAIT_SCAN_RETRY:
      if (millis() - _stateStartTime > 2000) {
        startScan();
      }
      break;
    case AP_STARTING:
      if (_apSubState == 1 && millis() - _stateStartTime > 500) {
        WiFi.setTxPower(WIFI_POWER_15dBm);
        WiFi.mode(WIFI_AP);
        _stateStartTime = millis();
        _apSubState = 2;
      } 
      else if (_apSubState == 2 && millis() - _stateStartTime > 200) {
        String apName = "Light_" + _hostname;
        WiFi.softAP(apName.c_str(), "12345678", 6, false, 4);
        WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
        _stateStartTime = millis();
        _apSubState = 3;
        Serial.printf("AP SSID: %s\n", apName.c_str());
      }
      else if (_apSubState == 3 && millis() - _stateStartTime > 500) {
        if (MDNS.begin(_hostname.c_str())) {
          Serial.printf("mDNS: Started as %s.local\n", _hostname.c_str());
        }
        _state = AP_MODE;
        _lastScanAttempt = millis();
        Serial.println("WiFi: AP mode fully active");
      }
      break;
    case AP_MODE:
      // Periodically try to reconnect
      if (millis() - _lastScanAttempt > AP_RESCAN_INTERVAL) {
        Serial.println("WiFi: AP mode - attempting rescan...");
        WiFi.softAPdisconnect(true);
        WiFi.mode(WIFI_STA);
        _state = STARTING;
        _stateStartTime = millis();
      }
      break;
    default:
      Serial.println("WiFi: Unknown state! Recovering...");
      startScan();
      break;
  }
}

void WiFiManager::startScan() {
  Serial.println("WiFi: Starting network scan...");
  _state = SCANNING;
  _stateStartTime = millis();
  _isScanning = true;
  WiFi.scanNetworks(true); // Async scan
}

void WiFiManager::processScanResults() {
  if (!_isScanning) return;
  
  int n = WiFi.scanComplete();
  
  if (n == WIFI_SCAN_RUNNING) {
    return; // Still scanning
  }
  
  if (n == WIFI_SCAN_FAILED) {
    Serial.println("WiFi: Scan failed, scheduled retry...");
    _isScanning = false;
    _state = WAIT_SCAN_RETRY;
    _stateStartTime = millis();
    return;
  }
  
  if (n == 0) {
    Serial.println("WiFi: No networks found");
    _isScanning = false;
    startAPMode();
    return;
  }
  
  Serial.printf("WiFi: Found %d networks\n", n);
  _isScanning = false;
  
  // First pass: Look for known networks
  String bestKnownSSID = "";
  int bestKnownRSSI = -1000;
  int bestKnownIndex = -1;
  
  for (int i = 0; i < n; i++) {
    String scannedSSID = WiFi.SSID(i);
    int scannedRSSI = WiFi.RSSI(i);
    
    // Check if this is a known network
    for (int j = 0; j < _networkCount; j++) {
      if (scannedSSID == _networks[j].ssid) {
        if (scannedRSSI > bestKnownRSSI) {
          bestKnownRSSI = scannedRSSI;
          bestKnownSSID = scannedSSID;
          bestKnownIndex = j;
        }
      }
    }
  }
  
  // If found known network, connect to it
  if (bestKnownIndex >= 0) {
    int bestRSSI = -1000;
    int bestAPIndex = -1;
    
    // Знаходимо найкращу точку доступу (BSSID) для цього SSID
    for (int i = 0; i < n; i++) {
      if (WiFi.SSID(i) == _networks[bestKnownIndex].ssid) {
        if (WiFi.RSSI(i) > bestRSSI) {
          bestRSSI = WiFi.RSSI(i);
          bestAPIndex = i;
        }
      }
    }

    if (bestAPIndex >= 0) {
      Serial.printf("WiFi: Found known network: %s (RSSI: %d, Channel: %d, BSSID: %s)\n", 
                    WiFi.SSID(bestAPIndex).c_str(), WiFi.RSSI(bestAPIndex), 
                    WiFi.channel(bestAPIndex), WiFi.BSSIDstr(bestAPIndex).c_str());
      
      _currentNetworkIndex = bestKnownIndex;
      _currentPowerIndex = 0;
      WiFi.scanDelete();
      connectToNetwork(_networks[bestKnownIndex].ssid, _networks[bestKnownIndex].password, false, 
                       WiFi.channel(bestAPIndex), WiFi.BSSID(bestAPIndex));
      return;
    }
  }
  
  // Second pass: Look for open networks with good signal
  String bestOpenSSID = "";
  int bestOpenRSSI = -1000;
  int bestOpenAPIndex = -1;
  
  for (int i = 0; i < n; i++) {
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
      int rssi = WiFi.RSSI(i);
      if (rssi > MIN_RSSI_OPEN && rssi > bestOpenRSSI) {
        bestOpenRSSI = rssi;
        bestOpenAPIndex = i;
      }
    }
  }
  
  WiFi.scanDelete();
  
  if (bestOpenAPIndex >= 0) {
    Serial.printf("WiFi: Found open network: %s (RSSI: %d, BSSID: %s)\n", 
                  WiFi.SSID(bestOpenAPIndex).c_str(), bestOpenRSSI, WiFi.BSSIDstr(bestOpenAPIndex).c_str());
    _currentPowerIndex = 0;
    connectToNetwork(WiFi.SSID(bestOpenAPIndex).c_str(), "", true, 
                     WiFi.channel(bestOpenAPIndex), WiFi.BSSID(bestOpenAPIndex));
  } else {
    Serial.println("WiFi: No suitable networks found");
    startAPMode();
  }
}

void WiFiManager::connectToNetwork(const char* ssid, const char* password, bool isOpen, int32_t channel, const uint8_t* bssid) {
  Serial.printf("WiFi: Preparing connection to %s [Power: %d]\n", ssid, _currentPowerIndex);
  
  _currentSSID = ssid;
  _pendingPassword = password;
  _isOpenNetwork = isOpen;
  _currentChannel = channel;
  if (bssid) {
    memcpy(_currentBSSID, bssid, 6);
  } else {
    memset(_currentBSSID, 0, 6);
    _currentChannel = 0; // Якщо немає BSSID, скидаємо канал для звичайного підключення
  }
  
  _state = DISCONNECTING;
  _stateStartTime = millis();
  
  WiFi.disconnect(true);
}

void WiFiManager::checkConnection() {
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi: Connected!");
    Serial.printf("IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Hostname: %s\n", _hostname.c_str());
    
    // Start mDNS
    if (MDNS.begin(_hostname.c_str())) {
        Serial.printf("mDNS: Started as %s.local\n", _hostname.c_str());
          MDNS.addService("http", "tcp", 80);
    } else {
      Serial.println("mDNS: Failed to start");
    }
    _state = CONNECTED;
    return;
  }
  
  // Check timeout
  if (millis() - _stateStartTime > CONNECT_TIMEOUT) {
    Serial.println("WiFi: Connection timeout");
    
    // Try next power level
    _currentPowerIndex++;
    if (_currentPowerIndex < POWER_LEVELS) {
      connectToNetwork(_currentSSID.c_str(), 
                      _isOpenNetwork ? "" : _networks[_currentNetworkIndex].password,
                      _isOpenNetwork);
      return;
    }
    
    // If this was a known network, try next network
    if (!_isOpenNetwork) {
      _currentNetworkIndex++;
      if (_currentNetworkIndex < _networkCount) {
        _currentPowerIndex = 0;
        connectToNetwork(_networks[_currentNetworkIndex].ssid,
                        _networks[_currentNetworkIndex].password,
                        false);
        return;
      }
    }
    
    // All attempts failed, start AP mode
    Serial.println("WiFi: All connection attempts failed");
    startAPMode();
  }
}

void WiFiManager::handleDisconnection() {
  Serial.println("WiFi: Attempting to reconnect...");
  _currentPowerIndex = 0;
  
  // Try to reconnect to the same network first
  if (_currentSSID.length() > 0) {
    if (_isOpenNetwork) {
      connectToNetwork(_currentSSID.c_str(), "", true, _currentChannel, _currentBSSID);
    } else {
      connectToNetwork(_networks[_currentNetworkIndex].ssid,
                      _networks[_currentNetworkIndex].password,
                      false, _currentChannel, _currentBSSID);
    }
  } else {
    // No previous network, start scan
    startScan();
  }
}

void WiFiManager::startAPMode() {
  Serial.println("WiFi: Switching to AP initialization...");
  
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  
  _state = AP_STARTING;
  _apSubState = 1;
  _stateStartTime = millis();
}

String WiFiManager::generateHostname() {
  uint64_t chipid = ESP.getEfuseMac(); // 6 байтів MAC
  char hostname[32];
#ifdef WIFI_HOSTNAME_PREFIX
  const char* prefix = WIFI_HOSTNAME_PREFIX;
#else
  const char* prefix = "light";
#endif
  snprintf(hostname, sizeof(hostname), "%s-%02X%02X%02X",
           prefix,
           (uint8_t)(chipid >> 24) & 0xFF,
           (uint8_t)(chipid >> 32) & 0xFF,
           (uint8_t)(chipid >> 40) & 0xFF);
  return String(hostname);
}
