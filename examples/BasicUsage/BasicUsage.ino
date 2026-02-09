#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ElegantOTA.h>
#include "secrets.h"
#include "WiFiManager.h"
#include "AuthenticationMiddleware.h"

// Globals
AsyncWebServer server(80);
AuthenticationMiddleware authMiddleware;
WiFiManager wifiManager;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- Starting BasicUsage Example ---");

  // 1. WiFi
  wifiManager.begin();

  // 2. Web Server & Auth
  authMiddleware.begin();
  server.addMiddleware(&authMiddleware);

  // Simple test page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "<h1>ESPWebUtils Example</h1><p>WiFi and Auth working!</p>");
  });

  ElegantOTA.begin(&server);
  server.begin();
  Serial.println("Web server started");
}

void loop() {
  wifiManager.tick();
  ElegantOTA.loop();
}
