#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// Pin Definitions
#define RELAY_PIN_1 D0  // Relay 1 control pin (Normally Open)
#define RELAY_PIN_2 D1  // Relay 2 control pin (Normally Open)
#define PZEM1_RX_PIN D3 // PZEM1 RX pin
#define PZEM1_TX_PIN D4 // PZEM1 TX pin
#define PZEM2_RX_PIN D5 // PZEM2 RX pin
#define PZEM2_TX_PIN D6 // PZEM2 TX pin
#define RST_BTN_PIN D7  // Reset button pin for configuration reset

// System Constants
#define CONFIG_CHECK_INTERVAL 10000       // Check configuration every 10 seconds
#define SENSOR_READING_INTERVAL 5000      // Read sensors every 5 seconds
#define MQTT_PUBLISH_INTERVAL 5000        // Publish data every 5 seconds (changed from 30000)
#define CONFIG_API_ENDPOINT "https://fdrixd24t3.sharedwithexpose.com/api/config"
#define DEVICE_UUID "esp-48E72958C566"
#define AP_NAME "Courant-Setup-"         // Will be appended with chip ID
#define AP_PASSWORD "setuppassword"       // Default AP password for config mode
#define CONFIG_PORTAL_TIMEOUT 180         // Timeout for config portal in seconds
#define MAX_SAMPLES 6                     // Store 6 samples for 30s average (with 5s interval)

// File paths for LittleFS
#define WIFI_CONFIG_FILE "/wifi_config.json"
#define DEVICE_CONFIG_FILE "/device_config.json"

// MQTT Constants
#define MQTT_BROKER "broker.hivemq.com"      // Public MQTT broker
#define MQTT_PORT 1883
#define MQTT_TOPIC_ALERTS "courant/device/esp-48E72958C566/alerts"
#define MQTT_TOPIC_READINGS "courant/device/esp-48E72958C566/readings"
#define MQTT_CLIENT_ID "courant-power-monitor"

// Debug settings
#define DEBUG_ENABLED true

#endif // CONFIG_H