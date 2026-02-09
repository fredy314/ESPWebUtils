#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <WiFi.h>
#include "esp_wifi.h"
#include <ESPmDNS.h>
#include "secrets_config.h"

class WiFiManager {
public:
  // Constructor with all parameters
  WiFiManager(
    WiFiNetwork* networks, 
    int networkCount,
    const char* hostname,
    IPAddress localIP,
    IPAddress gateway,
    IPAddress subnet,
    IPAddress primaryDNS,
    IPAddress secondaryDNS
  );

  // Constructor using values from secrets.h
  WiFiManager();
  
  void begin();
  void tick();
  
  bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
  IPAddress getIP() const { return WiFi.localIP(); }
  
private:
  // Configuration
  WiFiNetwork* _networks;
  int _networkCount;
  String _hostname;
  IPAddress _localIP;
  IPAddress _gateway;
  IPAddress _subnet;
  IPAddress _primaryDNS;
  IPAddress _secondaryDNS;
  
  // State
  enum State {
    SCANNING,
    CONNECTING_KNOWN,
    CONNECTING_OPEN,
    CONNECTED,
    AP_MODE
  };
  
  State _state;
  unsigned long _stateStartTime;
  unsigned long _lastScanAttempt;
  
  // Connection attempts
  int _currentNetworkIndex;
  int _currentPowerIndex;
  String _currentSSID;
  bool _isOpenNetwork;
  bool _isScanning;
  
  // Power levels
  static const int POWER_LEVELS = 4;
  wifi_power_t _powerSteps[POWER_LEVELS] = {
    WIFI_POWER_8_5dBm,
    WIFI_POWER_11dBm,
    WIFI_POWER_13dBm,
    WIFI_POWER_15dBm
  };
  
  // AP mode settings
  static const unsigned long AP_RESCAN_INTERVAL = 10 * 60 * 1000; // 10 minutes
  static const unsigned long CONNECT_TIMEOUT = 10000; // 10 seconds
  static const int MIN_RSSI_OPEN = -75; // Minimum signal strength for open networks
  
  // Private methods
  void startScan();
  void processScanResults();
  void connectToNetwork(const char* ssid, const char* password, bool isOpen = false);
  void startAPMode();
  void checkConnection();
  void handleDisconnection();
  String generateHostname();
};

#endif // WIFI_MANAGER_H
