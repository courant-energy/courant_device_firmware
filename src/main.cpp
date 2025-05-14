#include <Arduino.h>
#include <LittleFS.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "../include/wifi_manager.h"
#include "../include/config_manager.h"
#include "../include/sensor_manager.h"
#include "../include/relay_manager.h"
#include "../include/mqtt_manager.h"

// System managers
WiFiManager wifiManager;
ConfigManager configManager;
SensorManager sensorManager;
RelayManager relayManager;
MqttManager mqttManager;

// Timing variables
unsigned long lastPublishTime = 0;
unsigned long lastPowerCheckTime = 0;
bool resetButtonPressed = false;
unsigned long resetButtonPressTime = 0;

// Forward declarations
void checkPowerThresholds();
void checkResetButton();
void publishSensorData();
void setupResetButton();

void setup()
{
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(1000); // Short delay to ensure serial is ready

  Serial.println("\n\n");
  Serial.println("===================================");
  Serial.println("Courant Power Monitoring System");
  Serial.println("===================================");

  // Initialize LittleFS
  if (!LittleFS.begin())
  {
    Serial.println("[System] Failed to mount file system");
  }

  // Setup reset button pin
  setupResetButton();

  // Initialize relay control
  relayManager.begin();

  // Initialize WiFi and connect
  bool wifiConnected = wifiManager.begin();

  // Only continue with normal operation if WiFi is connected
  if (wifiConnected)
  {
    // Initialize configuration with device ID from MAC address
    String deviceId = DEVICE_UUID; // Use the defined UUID
    configManager.begin(deviceId);

    // Initialize sensor manager
    sensorManager.begin();

    // Initialize MQTT client
    mqttManager.begin();

    // Set initial relay states based on config
    relayManager.setRelay1(configManager.getSocket1Config().isOn);
    relayManager.setRelay2(configManager.getSocket2Config().isOn);

    Serial.println("[System] Initialization complete, starting main loop");
  }
  else
  {
    Serial.println("[System] WiFi in config mode, awaiting configuration");
  }
}

void loop()
{
  // Handle WiFi connection
  wifiManager.handleConnection();

  // Check reset button state
  checkResetButton();

  // Only proceed with normal operations if not in config mode
  if (!wifiManager.isInConfigMode())
  {
    // Update configuration (periodically checks API)
    configManager.update();

    // Update sensor readings
    if (sensorManager.update())
    {
      // Check if power readings exceed thresholds
      checkPowerThresholds();
    }

    // Update MQTT connection
    mqttManager.update();

    // Publish sensor data periodically
    publishSensorData();

    // Update relay states based on latest config
    relayManager.setRelay1(configManager.getSocket1Config().isOn);
    relayManager.setRelay2(configManager.getSocket2Config().isOn);
  }

  // Small delay to prevent watchdog issues
  delay(10);
}

void setupResetButton()
{
  pinMode(RST_BTN_PIN, INPUT_PULLUP);
  Serial.println("[System] Reset button configured on pin " + String(RST_BTN_PIN));
}

void checkResetButton()
{
  // Read the reset button (active low with pullup)
  bool buttonState = digitalRead(RST_BTN_PIN) == LOW;

  // Check if button just pressed
  if (buttonState && !resetButtonPressed)
  {
    resetButtonPressed = true;
    resetButtonPressTime = millis();
    Serial.println("[System] Reset button pressed, hold for 5 seconds to reset config");
  }

  // Check if button released
  if (!buttonState && resetButtonPressed)
  {
    resetButtonPressed = false;
    Serial.println("[System] Reset button released");
  }

  // Check if button held for reset duration (5 seconds)
  if (resetButtonPressed && (millis() - resetButtonPressTime > 5000))
  {
    Serial.println("[System] Reset button held for 5 seconds, performing factory reset...");

    // Reset all configurations
    wifiManager.resetConfig();
    configManager.resetConfig();

    Serial.println("[System] Factory reset complete. Rebooting...");
    delay(1000);
    ESP.restart();
  }
}

void checkPowerThresholds()
{
  // Only check if it's time to do so
  unsigned long currentTime = millis();
  if (currentTime - lastPowerCheckTime < 1000)
  { // Check at most once per second
    return;
  }
  lastPowerCheckTime = currentTime;

  // Get sensor data
  SensorData sensor1Data = sensorManager.getSensor1Data();
  SensorData sensor2Data = sensorManager.getSensor2Data();

  // Get threshold configurations
  SocketConfig socket1Config = configManager.getSocket1Config();
  SocketConfig socket2Config = configManager.getSocket2Config();
  // Check Socket 1 power threshold
  if (sensor1Data.isValid && sensor1Data.power > socket1Config.maximumPower && socket1Config.isOn)
  {
    Serial.println("[Alert] Socket 1 power exceeded threshold! " +
                   String(sensor1Data.power) + "W > " +
                   String(socket1Config.maximumPower) + "W");

    // Send alert via MQTT
    mqttManager.publishAlert(1, sensor1Data.power, socket1Config.maximumPower);
    
    // Turn off the relay
    relayManager.emergencyShutdown(1);
    
    // Refetch configuration from server after emergency shutdown
    if (WiFi.isConnected()) {
      Serial.println("[Config] Refetching configuration after emergency shutdown");
      configManager.fetchConfig();
    }
  }
  // Check Socket 2 power threshold
  if (sensor2Data.isValid && sensor2Data.power > socket2Config.maximumPower && socket2Config.isOn)
  {
    Serial.println("[Alert] Socket 2 power exceeded threshold! " +
                   String(sensor2Data.power) + "W > " +
                   String(socket2Config.maximumPower) + "W");

    // Turn off the relay
    relayManager.emergencyShutdown(2);

    // Send alert via MQTT
    mqttManager.publishAlert(2, sensor2Data.power, socket2Config.maximumPower);
    
    // Refetch configuration from server after emergency shutdown
    if (WiFi.isConnected()) {
      Serial.println("[Config] Refetching configuration after emergency shutdown");
      configManager.fetchConfig();
    }
  }
}

void publishSensorData()
{
  unsigned long currentTime = millis();

  // Only publish when enough time has elapsed
  if (currentTime - lastPublishTime >= MQTT_PUBLISH_INTERVAL)
  {
    lastPublishTime = currentTime;

    // Get complete sensor data readings
    SensorData socket1Data = sensorManager.getSensor1Data();
    SensorData socket2Data = sensorManager.getSensor2Data();

    // Publish to MQTT
    if (mqttManager.isConnected())
    {
      mqttManager.publishSensorData(socket1Data, socket2Data);

      if (DEBUG_ENABLED)
      {
        Serial.println("[System] Published sensor data: Socket1=" +
                       String(socket1Data.power) + "W (" +
                       String(socket1Data.voltage) + "V, " +
                       String(socket1Data.current) + "A), Socket2=" +
                       String(socket2Data.power) + "W (" +
                       String(socket2Data.voltage) + "V, " +
                       String(socket2Data.current) + "A)");
      }
    }
  }
}