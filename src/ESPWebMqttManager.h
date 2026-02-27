#ifndef ESP_WEB_MQTT_MANAGER_H
#define ESP_WEB_MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <vector>
#include <functional>

typedef std::function<String()> TGetter;
typedef std::function<void(String)> THandler;

struct MqttSensor {
    String topic;
    TGetter getter;
    unsigned long interval;
    unsigned long lastPublish;
    String lastValue;
    bool forcePublish;
};

struct MqttCommand {
    String topic;
    THandler handler;
};

class ESPWebMqttManager {
public:
    // idPrefix: наприклад "girlianda" -> буде "girlianda_A1B2"
    // namePrefix: наприклад "Гірлянда" -> буде "Гірлянда A1B2"
    ESPWebMqttManager(const char* idPrefix, const char* namePrefix);
    
    void setHosts(const char** hosts, int count);
    void begin();
    void loop();
    bool isConnected();
    String getDeviceId() const { return _deviceId; }

    // Додавання сенсора для автоматичної публікації
    void addSensor(String topic, TGetter getter, unsigned long interval = 60000);

    // Додавання обробника команди (підписка)
    void addCommand(String topic, THandler handler);

    // HA Discovery методи
    void publishAvailability(bool online);
    void publishDiscovery();
    void addHASensor(const char* id, const char* name, const char* deviceClass = nullptr, const char* unit = nullptr, const char* icon = nullptr);
    void addHABinarySensor(const char* id, const char* name, const char* deviceClass = nullptr, const char* icon = nullptr);
    void addHASwitch(const char* id, const char* name, const char* icon = nullptr);
    void addHALight(const char* id, const char* name);
    void addHAJsonLight(const char* id, const char* name, const char* effectsJson = nullptr);
    void addHASelect(const char* id, const char* name, const char* optionsJson);
    void addHANumber(const char* id, const char* name, float min, float max, const char* unit = nullptr, const char* icon = nullptr);

    /* // Proxy методи (для шлюзу ESP-NOW)
    void publishProxyAvailability(const char* deviceId, bool online);
    void publishProxyData(const char* deviceId, const char* component, const char* subTopic, const char* payload);
    void publishProxyDiscovery(const char* deviceId, const char* deviceName, const char* component, const char* id, const char* name, const char* type, const char* deviceClass = nullptr, const char* unit = nullptr, const char* icon = nullptr);
    */
private:
    String _clientId;
    String _deviceName;
    String _deviceId;
    
    WiFiClient _wifiClient;
    PubSubClient _mqttClient;
    
    const char** _mqttHosts;
    int _mqttHostCount;
    int _currentHostIndex;
    int _reconnectRetries;
    static const int MAX_RECONNECT_RETRIES = 3;
    static const int MQTT_PORT = 1883;

    unsigned long _lastReconnectAttempt;
    unsigned long _lastUpdateAll;
    bool _discoveryPublished;

    String _availabilityTopic;
    
    std::vector<MqttSensor> _sensors;
    std::vector<MqttCommand> _commands;
    struct HASensorDef {
        String id;
        String name;
        String deviceClass;
        String unit;
        String icon;
        String type; // sensor, switch, light, select, number, light_json
        String options; // для select
        float min, max; // для number
    };
    std::vector<HASensorDef> _haDefs;

    bool reconnect();
    void handleSensors();
    static void messageCallback(char* topic, byte* payload, unsigned int length);
    void processMessage(String topic, String payload);
    
    void publishHAConfig(const HASensorDef& def);
};

#endif
