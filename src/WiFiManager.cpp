#include "WiFiManager.h"
#include "secrets.h"
#include "esp_wifi.h"

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
    _isOpenNetwork(false),
    _isScanning(false) {
      
      esp_wifi_set_ps(WIFI_PS_NONE);
      esp_wifi_set_country_code("UA", true);
  // Generate hostname if not provided
  if (_hostname.length() == 0) {
    _hostname = generateHostname();
  }
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
        WiFi.begin(_currentSSID.c_str(), _pendingPassword.c_str());
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
    Serial.printf("WiFi: Found known network: %s (RSSI: %d)\n", bestKnownSSID.c_str(), bestKnownRSSI);
    _currentNetworkIndex = bestKnownIndex;
    _currentPowerIndex = 0;
    WiFi.scanDelete();
    connectToNetwork(_networks[bestKnownIndex].ssid, _networks[bestKnownIndex].password, false);
    return;
  }
  
  // Second pass: Look for open networks with good signal
  String bestOpenSSID = "";
  int bestOpenRSSI = -1000;
  
  for (int i = 0; i < n; i++) {
    if (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) {
      int rssi = WiFi.RSSI(i);
      if (rssi > MIN_RSSI_OPEN && rssi > bestOpenRSSI) {
        bestOpenRSSI = rssi;
        bestOpenSSID = WiFi.SSID(i);
      }
    }
  }
  
  WiFi.scanDelete();
  
  if (bestOpenSSID.length() > 0) {
    Serial.printf("WiFi: Found open network: %s (RSSI: %d)\n", bestOpenSSID.c_str(), bestOpenRSSI);
    _currentPowerIndex = 0;
    connectToNetwork(bestOpenSSID.c_str(), "", true);
  } else {
    Serial.println("WiFi: No suitable networks found");
    startAPMode();
  }
}

void WiFiManager::connectToNetwork(const char* ssid, const char* password, bool isOpen) {
  Serial.printf("WiFi: Preparing connection to %s [Power: %d]\n", ssid, _currentPowerIndex);
  
  _currentSSID = ssid;
  _pendingPassword = password;
  _isOpenNetwork = isOpen;
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
      connectToNetwork(_currentSSID.c_str(), "", true);
    } else {
      connectToNetwork(_networks[_currentNetworkIndex].ssid,
                      _networks[_currentNetworkIndex].password,
                      false);
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
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char hostname[32];
  snprintf(hostname, sizeof(hostname), "light-%02x%02x%02x", mac[3], mac[4], mac[5]);
  return String(hostname);
}
