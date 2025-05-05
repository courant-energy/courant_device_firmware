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
    Serial.println("[MQTT] Initializing MQTT client");
  }
  
  // Create a unique client ID using the MAC address
  clientId = MQTT_CLIENT_ID;
  clientId += "-";
  clientId += WiFi.macAddress();
  clientId.replace(":", "");

  // Create PubSubClient instance
  mqttClient = new PubSubClient(wifiClient);
  
  // Configure MQTT broker settings
  mqttClient->setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient->setCallback([this](char* topic, byte* payload, unsigned int length) {
    this->handleCallback(topic, payload, length);
  });
  
  // Try to connect
  reconnect();
  
  if (DEBUG_ENABLED) {
    if (connected) {
      Serial.println("[MQTT] Connected to MQTT broker");
      Serial.print("Broker: ");
      Serial.println(MQTT_BROKER);
      Serial.print("Client ID: ");
      Serial.println(clientId);
    } else {
      Serial.println("[MQTT] Failed to connect to MQTT broker, will retry later");
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
      Serial.println("[MQTT] WiFi not connected, skipping MQTT connection");
    }
    return false;
  }
  
  if (DEBUG_ENABLED) {
    Serial.print("[MQTT] Attempting MQTT connection...");
  }
  
  // Attempt to connect
  if (mqttClient->connect(clientId.c_str())) {
    connected = true;
    if (DEBUG_ENABLED) {
      Serial.println("connected");
    }
    
    // Subscribe to topics if needed
    // mqttClient->subscribe("device/commands");
    
    return true;
  } else {
    if (DEBUG_ENABLED) {
      Serial.print("failed, rc=");
      Serial.print(mqttClient->state());
      Serial.println(" will try again later");
    }
    return false;
  }
}

bool MqttManager::publishSensorData(float sensor1Power, float sensor2Power) {
  if (!connected) {
    if (DEBUG_ENABLED) {
      Serial.println("[MQTT] Not connected, can't publish data");
    }
    return false;
  }
  
  JsonDocument doc;
  doc["timestamp"] = millis();
  doc["socket1_power"] = sensor1Power;
  doc["socket2_power"] = sensor2Power;
  doc["device_id"] = clientId;
  
  String payload;
  serializeJson(doc, payload);
  
  bool success = mqttClient->publish(MQTT_TOPIC_READINGS, payload.c_str());
  
  if (DEBUG_ENABLED) {
    if (success) {
      Serial.println("[MQTT] Published sensor data: " + payload);
    } else {
      Serial.println("[MQTT] Failed to publish sensor data");
    }
  }
  
  return success;
}

bool MqttManager::publishAlert(uint8_t socketNumber, float currentPower, float maxPower) {
  if (!connected) {
    if (DEBUG_ENABLED) {
      Serial.println("[MQTT] Not connected, can't publish alert");
    }
    return false;
  }
  
  JsonDocument doc;
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
      Serial.println("[MQTT] Published alert: " + payload);
    } else {
      Serial.println("[MQTT] Failed to publish alert");
    }
  }
  
  return success;
}

void MqttManager::handleCallback(char* topic, byte* payload, unsigned int length) {
  // Process received MQTT messages here if needed
  if (DEBUG_ENABLED) {
    Serial.print("[MQTT] Message arrived on topic: ");
    Serial.println(topic);
    Serial.print("Payload: ");
    for (unsigned int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }
  
  // Add handling for specific topics as needed
}