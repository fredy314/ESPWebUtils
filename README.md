# ESPWebUtils

Бібліотека утиліт для ESP32: WiFi Manager, Authentication Middleware для веб-серверів та ESPWebMqttManager для зручної роботи з MQTT брокерами.

## Можливості

### WiFiManager
- 📶 Автоматичне підключення до найкращої мережі зі списку
- 🔓 Підключення до відкритих мереж (якщо немає мереж зі списку)
- 🔄 Автоматичне повторне підключення при втраті зв'язку
- 📡 Режим AP з повторним пошуком мереж кожні 10 хвилин
- 🌐 Динамічний hostname з mDNS
- ⚙️ Підтримка статичного IP або DHCP

### AuthenticationMiddleware
- 🔐 Digest Authentication для веб-сервера
- 🛡️ Захист сторінок від несанкціонованого доступу
- ⚙️ Гнучке налаштування через `secrets.h`
### ESPWebMqttManager
- 📡 Інтеграція з MQTT брокерами
- 🔄 Автоматичне перепідключення та підтримка кількох хостів (резервування)
- 🏠 Зручне створення сенсоров та світла для Home Assistant з авто-discover (HA MQTT Discovery)
- 🎛️ Зручна прив'язка лямбда-функцій до команд MQTT

## Встановлення

1. Скопіюйте папку `ESPWebUtils` в `~/Arduino/libraries/`
2. Перезапустіть Arduino IDE

## Використання

### 1. Створіть `secrets.h` у вашому проекті

```cpp
#ifndef SECRETS_H
#define SECRETS_H

#include "secrets_types.h"

// WiFi мережі (обов'язково)
inline WiFiNetwork wifiNetworks[] = {
  {"YourSSID1", "YourPassword1"},
  {"YourSSID2", "YourPassword2"}
};
inline const int wifiNetworkCount = sizeof(wifiNetworks) / sizeof(wifiNetworks[0]);
#define WIFI_NETWORKS_DEFINED

// ============================================
// MQTT налаштування (якщо використовується ESPWebMqttManager)
// ============================================
inline const char* mqttHosts[] = {"mqtt.lan", "192.168.0.150", "192.168.0.120"};
inline const int mqttHostCount = sizeof(mqttHosts) / sizeof(mqttHosts[0]);

// Опціонально - розкоментуйте та змініть за потреби:
// #define WIFI_HOSTNAME "my-device"
// #define STATIC_IP IPAddress(192, 168, 1, 100)
// #define AUTH_PASSWORD "my-password"

#include "secrets_config.h"

#endif
```

### 2. Створіть `MqttHelper.h` (Опціонально, для організації коду)

Це допоможе винести логіку ініціалізації MQTT в окремий файл. Дивіться `examples/BasicUsage/MqttHelper.h.example`.

### 3. Використайте в коді

```cpp
#include <WiFiManager.h>
#include <AuthenticationMiddleware.h>
#include <ESPWebMqttManager.h>
#include "secrets.h"

WiFiManager wifiManager;
AuthenticationMiddleware authMiddleware;
AsyncWebServer server(80);
ESPWebMqttManager mqttManager("basic_device", "Моє Базове Устаткування");

void setup() {
  // WiFi
  wifiManager.begin();
  
  // Auth
  authMiddleware.begin();
  server.addMiddleware(&authMiddleware);

  // MQTT
  mqttManager.setHosts(mqttHosts, mqttHostCount);
  mqttManager.addSensor("sensor1", []() { return String(random(20, 30)); }, 5000);
  mqttManager.begin();
  
  server.begin();
}

void loop() {
  wifiManager.tick();
  mqttManager.loop(); // Обов'язково викликати в loop
}
```

## Налаштування через secrets.h

Всі параметри опціональні (крім WiFi мереж). Якщо не вказані, використовуються значення за замовчуванням:

| Параметр | Опис | За замовчуванням |
|----------|------|------------------|
| `WIFI_HOSTNAME` | Hostname для mDNS | Генерується з MAC |
| `STATIC_IP` | Статичний IP | DHCP (0.0.0.0) |
| `GATEWAY_IP` | Gateway | 192.168.0.1 |
| `SUBNET_MASK` | Subnet mask | 255.255.255.0 |
| `PRIMARY_DNS` | Primary DNS | 8.8.8.8 |
| `SECONDARY_DNS` | Secondary DNS | 8.8.4.4 |
| `NTP_SERVER` | NTP сервер | pool.ntp.org |
| `GMT_OFFSET_SEC` | Часовий пояс | 7200 (UTC+2) |
| `AUTH_REALM` | Auth realm | "Smart Light Controller" |
| `AUTH_USERNAME` | Username | "admin" |
| `AUTH_PASSWORD` | Password | "admin" |
| `AUTH_FAIL_MESSAGE` | Повідомлення помилки | "Authentication failed" |

## Приклади

Дивіться папку `examples/BasicUsage/` для повного прикладу.

## Залежності

Ця бібліотека залежить від наступних:
- [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
- [PubSubClient](https://github.com/knolleary/pubsubclient) (Для ESPWebMqttManager)
- [ArduinoJson](https://arduinojson.org/) (Для підтримки JSON у ESPWebMqttManager та MQTT Discovery)

## Ліцензія

MIT
