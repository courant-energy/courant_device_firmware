#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <Arduino.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "config.h"

struct SocketConfig {
    uint8_t assignedId;
    float maximumPower;
    bool isOn;
    
    SocketConfig() : assignedId(0), maximumPower(0), isOn(false) {}
};

class ConfigManager {
private:
    SocketConfig socket1Config;
    SocketConfig socket2Config;
    unsigned long lastConfigCheck;
    String deviceUuid;
    
    bool loadFromLittleFS();
    bool saveToLittleFS();
    bool parseConfigJson(String jsonString);

public:
    ConfigManager();
    
    void begin(String deviceId);
    bool fetchConfig();
    void update();
    
    SocketConfig getSocket1Config() { return socket1Config; }
    SocketConfig getSocket2Config() { return socket2Config; }
    
    // Manually set values and save to LittleFS
    void setSocket1MaxPower(float power);
    void setSocket2MaxPower(float power);
    
    // Reset configuration
    void resetConfig();
};

#endif // CONFIG_MANAGER_H