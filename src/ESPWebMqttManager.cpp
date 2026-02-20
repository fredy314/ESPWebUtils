#include "ESPWebMqttManager.h"

static ESPWebMqttManager* _instance = nullptr;

ESPWebMqttManager::ESPWebMqttManager(const char* idPrefix, const char* namePrefix)
    : _mqttClient(_wifiClient), _mqttHosts(nullptr), _mqttHostCount(0), 
      _currentHostIndex(0), _reconnectRetries(0), _lastReconnectAttempt(0), 
      _lastUpdateAll(0), _discoveryPublished(false) 
{
    _instance = this;
    
    uint64_t mac = ESP.getEfuseMac();
    char suffix[7];
    sprintf(suffix, "%02X%02X%02X", 
            (uint8_t)(mac >> 24) & 0xFF, 
            (uint8_t)(mac >> 32) & 0xFF, 
            (uint8_t)(mac >> 40) & 0xFF);
            
    _deviceId = String(idPrefix) + "_" + suffix;
    _clientId = _deviceId; // Використовуємо один ID для клієнта та пристрою
    _deviceName = String(namePrefix) + " " + suffix;
    
    _availabilityTopic = String("homeassistant/") + _deviceId + "/availability";
}

void ESPWebMqttManager::setHosts(const char** hosts, int count) {
    _mqttHosts = hosts;
    _mqttHostCount = count;
}

void ESPWebMqttManager::begin() {
    _mqttClient.setCallback(messageCallback);
    _mqttClient.setBufferSize(1024); // Збільшено для довгих JSON конфігурацій HA
    _mqttClient.setSocketTimeout(2);
    reconnect();
    _lastReconnectAttempt = millis();
}

void ESPWebMqttManager::loop() {
    if (WiFi.status() != WL_CONNECTED) return;

    if (!_mqttClient.connected()) {
        unsigned long now = millis();
        if (now - _lastReconnectAttempt > 15000) {
            _lastReconnectAttempt = now;
            if (reconnect()) {
                _lastReconnectAttempt = 0;
            }
        }
    } else {
        _mqttClient.loop();
        handleSensors();
        
        unsigned long now = millis();
        if (now - _lastUpdateAll > 600000) { // Кожні 10 хв
            _lastUpdateAll = now;
            publishAvailability(true);
            publishDiscovery();
            for (auto& s : _sensors) s.forcePublish = true;
        }
    }
}

bool ESPWebMqttManager::isConnected() {
    return _mqttClient.connected();
}

void ESPWebMqttManager::addSensor(String topic, TGetter getter, unsigned long interval) {
    _sensors.push_back({topic, getter, interval, 0, "", true});
}

void ESPWebMqttManager::addCommand(String topic, THandler handler) {
    _commands.push_back({topic, handler});
    if (_mqttClient.connected()) {
        _mqttClient.subscribe(topic.c_str());
    }
}

bool ESPWebMqttManager::reconnect() {
    if (WiFi.status() != WL_CONNECTED || _mqttHostCount == 0) return false;

    _mqttClient.setServer(_mqttHosts[_currentHostIndex], MQTT_PORT);
    Serial.printf("MQTT: Connecting to %s...\n", _mqttHosts[_currentHostIndex]);
    
    if (_mqttClient.connect(_clientId.c_str(), _availabilityTopic.c_str(), 0, true, "offline")) {
        Serial.println("MQTT: Connected");
        _reconnectRetries = 0;
        publishAvailability(true);
        if (!_discoveryPublished) {
            publishDiscovery();
            _discoveryPublished = true;
        }
        // Перепідписка на команди
        for (const auto& cmd : _commands) {
            _mqttClient.subscribe(cmd.topic.c_str());
        }
        for (auto& s : _sensors) s.forcePublish = true;
        return true;
    }

    _reconnectRetries++;
    if (_reconnectRetries >= MAX_RECONNECT_RETRIES) {
        if (_mqttHostCount > 1) {
            _currentHostIndex = (_currentHostIndex + 1) % _mqttHostCount;
            _reconnectRetries = 0;
        }
    }
    return false;
}

void ESPWebMqttManager::handleSensors() {
    unsigned long now = millis();
    for (auto& s : _sensors) {
        if (s.forcePublish || (now - s.lastPublish > s.interval)) {
            String val = s.getter();
            if (s.forcePublish || val != s.lastValue) {
                if (_mqttClient.publish(s.topic.c_str(), val.c_str())) {
                    s.lastValue = val;
                    s.lastPublish = now;
                    s.forcePublish = false;
                }
            }
        }
    }
}

void ESPWebMqttManager::publishAvailability(bool online) {
    _mqttClient.publish(_availabilityTopic.c_str(), online ? "online" : "offline", true);
}

void ESPWebMqttManager::publishDiscovery() {
    for (const auto& def : _haDefs) {
        publishHAConfig(def);
    }
}

void ESPWebMqttManager::addHASensor(const char* id, const char* name, const char* deviceClass, const char* unit, const char* icon) {
    _haDefs.push_back({id, name, deviceClass ? deviceClass : "", unit ? unit : "", icon ? icon : "", "sensor", "", 0, 0});
}

void ESPWebMqttManager::addHASwitch(const char* id, const char* name, const char* icon) {
    _haDefs.push_back({id, name, "", "", icon ? icon : "", "switch", "", 0, 0});
}

void ESPWebMqttManager::addHALight(const char* id, const char* name) {
    _haDefs.push_back({id, name, "", "", "", "light", "", 0, 0});
}

void ESPWebMqttManager::addHAJsonLight(const char* id, const char* name, const char* effectsJson) {
    _haDefs.push_back({id, name, "", "", "", "light_json", effectsJson ? effectsJson : "", 0, 0});
}

void ESPWebMqttManager::addHASelect(const char* id, const char* name, const char* optionsJson) {
    _haDefs.push_back({id, name, "", "", "", "select", optionsJson, 0, 0});
}

void ESPWebMqttManager::addHANumber(const char* id, const char* name, float min, float max, const char* unit, const char* icon) {
    _haDefs.push_back({id, name, "", "", icon ? icon : "", "number", "", min, max});
}

void ESPWebMqttManager::publishHAConfig(const HASensorDef& def) {
    String component = def.type;
    if (component == "light_json") component = "light";

    String configTopic = String("homeassistant/") + component + "/" + _deviceId + "/" + def.id + "/config";
    String baseTopic = String("homeassistant/") + component + "/" + _deviceId + "/" + def.id;
    
    String config = "{";
    config += "\"name\":\"" + String(_deviceName) + " " + def.name + "\",";
    config += "\"unique_id\":\"" + String(_deviceId) + "_" + def.id + "\",";
    
    if (def.type == "sensor") {
        config += "\"state_topic\":\"" + baseTopic + "/state\",";
    } else if (def.type == "switch" || def.type == "light") {
        config += "\"state_topic\":\"" + baseTopic + "/state\",";
        config += "\"command_topic\":\"" + baseTopic + "/set\",";
    } else if (def.type == "light_json") {
        config += "\"state_topic\":\"" + baseTopic + "/state\",";
        config += "\"command_topic\":\"" + baseTopic + "/set\",";
        config += "\"schema\":\"json\",";
        config += "\"brightness\":true,";
        if (def.options != "") {
            config += "\"effect\":true,";
            config += "\"effect_list\":" + def.options+",";
        }
    } else if (def.type == "select") {
        config += "\"state_topic\":\"" + baseTopic + "/state\",";
        config += "\"command_topic\":\"" + baseTopic + "/set\",";
        config += "\"options\":" + def.options + ",";
    } else if (def.type == "number") {
        config += "\"state_topic\":\"" + baseTopic + "/state\",";
        config += "\"command_topic\":\"" + baseTopic + "/set\",";
        config += "\"min\":" + String(def.min) + ",\"max\":" + String(def.max) + ",";
    }

    if (def.deviceClass != "") config += "\"device_class\":\"" + def.deviceClass + "\",";
    if (def.unit != "") config += "\"unit_of_measurement\":\"" + def.unit + "\",";
    if (def.icon != "") config += "\"icon\":\"" + def.icon + "\",";
    
    config += "\"availability_topic\":\"" + _availabilityTopic + "\",";
    config += "\"device\":{";
    config += "\"identifiers\":[\"" + String(_deviceId) + "\"],";
    config += "\"name\":\"" + String(_deviceName) + "\",";
    config += "\"manufacturer\":\"Custom\",";
    config += "\"model\":\"ESPWebUtils Device\"";
    config += "}}";
    
    _mqttClient.publish(configTopic.c_str(), config.c_str(), true);
}
/*
void ESPWebMqttManager::publishProxyAvailability(const char* deviceId, bool online) {
    String topic = String("homeassistant/sensor/") + deviceId + "/availability";
    _mqttClient.publish(topic.c_str(), online ? "online" : "offline", true);
}

void ESPWebMqttManager::publishProxyData(const char* deviceId, const char* component, const char* subTopic, const char* payload) {
    String topic = String("homeassistant/") + component + "/" + deviceId + "/" + subTopic;
    _mqttClient.publish(topic.c_str(), payload);
}

void ESPWebMqttManager::publishProxyDiscovery(const char* deviceId, const char* deviceName, const char* component, const char* id, const char* name, const char* type, const char* deviceClass, const char* unit, const char* icon) {
    String configTopic = String("homeassistant/") + type + "/" + deviceId + "/" + id + "/config";
    String baseTopic = String("homeassistant/") + type + "/" + deviceId + "/" + id;
    String availabilityTopic = String("homeassistant/sensor/") + deviceId + "/availability";
    
    String config = "{";
    config += "\"name\":\"" + String(deviceName) + " " + name + "\",";
    config += "\"unique_id\":\"" + String(deviceId) + "_" + id + "\",";
    
    config += "\"state_topic\":\"" + baseTopic + "/state\",";
    if (String(type) == "switch" || String(type) == "light") {
        config += "\"command_topic\":\"" + baseTopic + "/set\",";
    }

    if (deviceClass && strlen(deviceClass) > 0) config += "\"device_class\":\"" + String(deviceClass) + "\",";
    if (unit && strlen(unit) > 0) config += "\"unit_of_measurement\":\"" + String(unit) + "\",";
    if (icon && strlen(icon) > 0) config += "\"icon\":\"" + String(icon) + "\",";
    
    config += "\"availability_topic\":\"" + availabilityTopic + "\",";
    config += "\"device\":{";
    config += "\"identifiers\":[\"" + String(deviceId) + "\"],";
    config += "\"name\":\"" + String(deviceName) + "\",";
    config += "\"manufacturer\":\"Custom (via Gateway)\",";
    config += "\"model\":\"ESPWebUtils Node\"";
    config += "}}";
    
    _mqttClient.publish(configTopic.c_str(), config.c_str(), true);
}
*/
void ESPWebMqttManager::messageCallback(char* topic, byte* payload, unsigned int length) {
    if (_instance) {
        String p = "";
        for (unsigned int i = 0; i < length; i++) p += (char)payload[i];
        _instance->processMessage(String(topic), p);
    }
}

void ESPWebMqttManager::processMessage(String topic, String payload) {
    for (const auto& cmd : _commands) {
        if (cmd.topic == topic) {
            cmd.handler(payload);
        }
    }
}
