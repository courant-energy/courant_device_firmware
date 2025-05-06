#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "config.h"
#include "sensor_manager.h"

class MqttManager {
private:
    WiFiClient wifiClient;
    PubSubClient* mqttClient;
    String clientId;
    bool connected;
    unsigned long lastReconnectAttempt;
    unsigned long lastPublishTime;
    
    bool reconnect();
    
public:
    MqttManager();
    ~MqttManager();
    
    bool begin();
    void update();
    bool isConnected() { return connected; }
    
    // Publish methods
    bool publishSensorData(const SensorData& sensor1Data, const SensorData& sensor2Data);
    bool publishAlert(uint8_t socketNumber, float currentPower, float maxPower);
    
    // Handle MQTT events
    void handleCallback(char* topic, byte* payload, unsigned int length);
};

#endif // MQTT_MANAGER_H