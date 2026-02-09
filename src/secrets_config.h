#ifndef SECRETS_CONFIG_H
#define SECRETS_CONFIG_H

#include "secrets_types.h"

// ============================================
// DEFAULTS - можна перевизначити в secrets.h
// ============================================

// Hostname для mDNS (за замовчуванням генерується з MAC)
#ifndef WIFI_HOSTNAME
#define WIFI_HOSTNAME ""
#endif

// NTP Configuration
#ifndef NTP_SERVER
#define NTP_SERVER "pool.ntp.org"
#endif

#ifndef GMT_OFFSET_SEC
#define GMT_OFFSET_SEC 7200  // UTC+2 (Kyiv Winter)
#endif

#ifndef DAYLIGHT_OFFSET_SEC
#define DAYLIGHT_OFFSET_SEC 3600  // +1h for Summer
#endif

// Static IP Configuration (0.0.0.0 = DHCP)
#ifndef STATIC_IP
#define STATIC_IP IPAddress(0, 0, 0, 0)
#endif

#ifndef GATEWAY_IP
#define GATEWAY_IP IPAddress(192, 168, 0, 1)
#endif

#ifndef SUBNET_MASK
#define SUBNET_MASK IPAddress(255, 255, 255, 0)
#endif

#ifndef PRIMARY_DNS
#define PRIMARY_DNS IPAddress(8, 8, 8, 8)
#endif

#ifndef SECONDARY_DNS
#define SECONDARY_DNS IPAddress(8, 8, 4, 4)
#endif

// Authentication Settings
#ifndef AUTH_REALM
#define AUTH_REALM "Smart Light Controller"
#endif

#ifndef AUTH_USERNAME
#define AUTH_USERNAME "admin"
#endif

#ifndef AUTH_PASSWORD
#define AUTH_PASSWORD "admin"
#endif

#ifndef AUTH_FAIL_MESSAGE
#define AUTH_FAIL_MESSAGE "Authentication failed"
#endif

// ============================================
// Застосування значень
// ============================================

inline const char* hostname = WIFI_HOSTNAME;
inline const char* ntpServer = NTP_SERVER;
inline const long gmtOffset_sec = GMT_OFFSET_SEC;
inline const int daylightOffset_sec = DAYLIGHT_OFFSET_SEC;

inline IPAddress local_IP = STATIC_IP;
inline IPAddress gateway = GATEWAY_IP;
inline IPAddress subnet = SUBNET_MASK;
inline IPAddress primaryDNS = PRIMARY_DNS;
inline IPAddress secondaryDNS = SECONDARY_DNS;

inline const char* authRealm = AUTH_REALM;
inline const char* authUsername = AUTH_USERNAME;
inline const char* authPassword = AUTH_PASSWORD;
inline const char* authFailMessage = AUTH_FAIL_MESSAGE;

#endif // SECRETS_CONFIG_H
