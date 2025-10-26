#include "../include/mqtt_manager.h"

MqttManager::MqttManager() {
  mqttClient = nullptr;
  connected = false;
  lastReconnectAttempt = 0;
  lastPublishTime = 0;
}

MqttManager::~MqttManager() {
  if (mqttClient) {
    mqttClient->disconnect();
    delete mqttClient;
  }
}

bool MqttManager::begin() {
  if (DEBUG_ENABLED) {
    Serial.println(F("[MQTT] Initializing MQTT client"));
  }
  
  // Create a unique client ID using the MAC address
  clientId = DEVICE_UUID;

  wifiClient.setInsecure();  

  // Create PubSubClient instance
  mqttClient = new PubSubClient(wifiClient);
  
  // Set a larger buffer size to accommodate the nested JSON data (default is only 256 bytes)
  mqttClient->setBufferSize(1024);
  
  // Configure MQTT broker settings
  mqttClient->setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {
    this->handleCallback(topic, payload, length);
  });
  
  // Try to connect
  reconnect();
  
  if (DEBUG_ENABLED) {
    if (connected) {
      Serial.println(F("[MQTT] Connected to MQTT broker"));
      Serial.print(F("Broker: "));
      Serial.println(MQTT_BROKER);
      Serial.print(F("Client ID: "));
      Serial.println(clientId);
    } else {
      Serial.println(F("[MQTT] Failed to connect to MQTT broker, will retry later"));
    }
  }
  
  return connected;
}

void MqttManager::update() {
  // Check if connection is active
  if (!mqttClient->connected()) {
    connected = false;
    
    // Try to reconnect if it's time
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 30000) {  // Try every 30 seconds
      lastReconnectAttempt = now;
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    connected = true;
    mqttClient->loop();  // Process MQTT messages
  }
}

bool MqttManager::reconnect() {
  if (!WiFi.isConnected()) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[MQTT] WiFi not connected, skipping MQTT connection"));
    }
    return false;
  }
  
  if (DEBUG_ENABLED) {
    Serial.print(F("[MQTT] Attempting MQTT connection..."));
  }
  
  // Attempt to connect
  if (mqttClient->connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD)) {
    connected = true;
    if (DEBUG_ENABLED) {
      Serial.println(F("connected"));
    }
    
    // Subscribe to topics if needed
    // mqttClient->subscribe("device/commands");
    
    return true;
  } else {
    if (DEBUG_ENABLED) {
      Serial.print("failed, rc=");
      Serial.print(mqttClient->state());
      Serial.println(F(" will try again later"));
    }
    return false;
  }
}



void MqttManager::disconnect() {
  if (mqttClient && mqttClient->connected()) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[MQTT] Disconnecting from MQTT broker"));
    }
    mqttClient->disconnect();
    connected = false;
  }
}

bool MqttManager::publishSensorData(const SensorData& sensor1Data, const SensorData& sensor2Data) {
  if (!connected) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[MQTT] Not connected, can't publish data"));
    }
    return false;
  }
  
  StaticJsonDocument<768> doc;  // Optimized size for sensor data
  doc["timestamp"] = millis();
  doc["device_id"] = clientId;
  
  // Socket 1 data - using recommended syntax instead of createNestedObject
  doc["socket1"].to<JsonObject>();
  doc["socket1"]["socket_id"] = 1;  // Add socket_id field
  doc["socket1"]["voltage"] = sensor1Data.voltage;
  doc["socket1"]["current"] = sensor1Data.current;
  doc["socket1"]["power"] = sensor1Data.power;
  doc["socket1"]["energy"] = sensor1Data.energy;
  doc["socket1"]["frequency"] = sensor1Data.frequency;
  doc["socket1"]["power_factor"] = sensor1Data.powerFactor;
  doc["socket1"]["valid"] = sensor1Data.isValid;
  
  // Socket 2 data - using recommended syntax instead of createNestedObject
  doc["socket2"].to<JsonObject>();
  doc["socket2"]["socket_id"] = 2;  // Add socket_id field
  doc["socket2"]["voltage"] = sensor2Data.voltage;
  doc["socket2"]["current"] = sensor2Data.current;
  doc["socket2"]["power"] = sensor2Data.power;
  doc["socket2"]["energy"] = sensor2Data.energy;
  doc["socket2"]["frequency"] = sensor2Data.frequency;
  doc["socket2"]["power_factor"] = sensor2Data.powerFactor;
  doc["socket2"]["valid"] = sensor2Data.isValid;
  
  String payload;
  serializeJson(doc, payload);
  
  // Print payload size for debugging
  if (DEBUG_ENABLED) {
    Serial.print(F("[MQTT] Payload size: "));
    Serial.print(payload.length());
    Serial.println(F(" bytes"));
  }
  
  bool success = mqttClient->publish(MQTT_TOPIC_READINGS, payload.c_str());
  
  if (DEBUG_ENABLED) {
    if (success) {
      Serial.print(F("[MQTT] Published sensor data: "));
      Serial.println(payload);
    } else {
      Serial.println(F("[MQTT] Failed to publish sensor data"));
      Serial.print(F("[MQTT] MQTT client state: "));
      Serial.println(mqttClient->state());
    }
  }
  
  return success;
}

bool MqttManager::publishAlert(uint8_t socketNumber, float currentPower, float maxPower) {
  if (!connected) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[MQTT] Not connected, can't publish alert"));
    }
    return false;
  }
  
  StaticJsonDocument<256> doc;  // Small alert message
  doc["timestamp"] = millis();
  doc["socket"] = socketNumber;
  doc["current_power"] = currentPower;
  doc["max_power"] = maxPower;
  doc["device_id"] = clientId;
  doc["alert_type"] = "power_exceeded";
  
  String payload;
  serializeJson(doc, payload);
  
  bool success = mqttClient->publish(MQTT_TOPIC_ALERTS, payload.c_str());
  
  if (DEBUG_ENABLED) {
    if (success) {
      Serial.print(F("[MQTT] Published alert: "));
      Serial.println(payload);
    } else {
      Serial.println(F("[MQTT] Failed to publish alert"));
    }
  }
  
  return success;
}

void MqttManager::handleCallback(char* topic, byte* payload, unsigned int length) {
  // Process received MQTT messages here if needed
  if (DEBUG_ENABLED) {
    Serial.print(F("[MQTT] Message arrived on topic: "));
    Serial.println(topic);
    Serial.print("Payload: ");
    for (unsigned int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }
  
  // Add handling for specific topics as needed
}