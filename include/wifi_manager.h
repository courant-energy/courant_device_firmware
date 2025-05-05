#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "config.h"

class WiFiManager {
private:
    AsyncWebServer *server;
    String ssid;
    String password;
    bool isConfigMode;
    bool isConnected;
    unsigned long lastReconnectAttempt;
    
    void startAccessPoint();
    void setupConfigPortal();
    bool loadWifiConfig();
    void saveWifiConfig(String ssid, String password);

public:
    WiFiManager();
    ~WiFiManager();
    
    bool begin();
    bool isInConfigMode() { return isConfigMode; }
    bool isWiFiConnected() { return isConnected; }
    void handleConnection();
    void resetConfig();
    String getMacAddress();
};

#endif // WIFI_MANAGER_H