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
unsigned long lastConfigCheckTime = 0;
bool resetButtonPressed = false;
unsigned long resetButtonPressTime = 0;

// Forward declarations
void checkPowerThresholds();
void checkResetButton();
void publishSensorData();
void setupResetButton();
bool updateConfiguration();
void checkPeriodicConfigUpdate();

void setup()
{
  // Initialize serial for debugging
  Serial.begin(115200);
  delay(1000); // Short delay to ensure serial is ready

  Serial.println(F("\n\n"));
  Serial.println(F("==================================="));
  Serial.println(F("Courant Power Monitoring System"));
  Serial.println(F("==================================="));

  // Initialize LittleFS
  if (!LittleFS.begin())
  {
    Serial.println(F("[System] Failed to mount file system"));
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

    Serial.println(F("[System] Initialization complete, starting main loop"));
    if (DEBUG_ENABLED) {
      Serial.print(F("[System] Free heap after initialization: "));
      Serial.print(ESP.getFreeHeap());
      Serial.println(F(" bytes"));
    }
  }
  else
  {
    Serial.println(F("[System] WiFi in config mode, awaiting configuration"));
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
    // Check for periodic configuration updates (with MQTT coordination)
    checkPeriodicConfigUpdate();

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

bool updateConfiguration()
{
  // 1. Temporarily disconnect MQTT to free TLS resources
  if (mqttManager.isConnected()) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[System] Temporarily disconnecting MQTT for configuration update"));
    }
    mqttManager.disconnect();
    // Short delay to ensure socket closure is processed
    delay(50);
  }

  // 2. Execute the secure HTTPS request
  bool success = configManager.fetchConfig();

  // 3. MQTT will automatically reconnect on next update() call in main loop
  if (DEBUG_ENABLED) {
    if (success) {
      Serial.println(F("[System] Configuration update successful, MQTT will reconnect"));
    } else {
      Serial.println(F("[System] Configuration update failed, MQTT will reconnect"));
    }
  }

  return success;
}

void checkPeriodicConfigUpdate()
{
  unsigned long currentTime = millis();
  
  // Check if it's time to fetch new config (every 10 seconds as defined in CONFIG_CHECK_INTERVAL)
  if (currentTime - lastConfigCheckTime >= CONFIG_CHECK_INTERVAL) {
    lastConfigCheckTime = currentTime;
    
    if (WiFi.isConnected()) {
      if (DEBUG_ENABLED) {
        Serial.println(F("[Config] Performing periodic configuration check"));
      }
      updateConfiguration();
    }
  }
}

void setupResetButton()
{
  pinMode(RST_BTN_PIN, INPUT_PULLUP);
  Serial.print(F("[System] Reset button configured on pin "));
  Serial.println(RST_BTN_PIN);
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
    Serial.println(F("[System] Reset button pressed, hold for 5 seconds to reset config"));
  }

  // Check if button released
  if (!buttonState && resetButtonPressed)
  {
    resetButtonPressed = false;
    Serial.println(F("[System] Reset button released"));
  }

  // Check if button held for reset duration (5 seconds)
  if (resetButtonPressed && (millis() - resetButtonPressTime > 5000))
  {
    Serial.println(F("[System] Reset button held for 5 seconds, performing factory reset..."));

    // Reset all configurations
    wifiManager.resetConfig();
    configManager.resetConfig();

    Serial.println(F("[System] Factory reset complete. Rebooting..."));
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
    Serial.print(F("[Alert] Socket 1 power exceeded threshold! "));
    Serial.print(sensor1Data.power);
    Serial.print(F("W > "));
    Serial.print(socket1Config.maximumPower);
    Serial.println(F("W"));

    // Send alert via MQTT
    mqttManager.publishAlert(1, sensor1Data.power, socket1Config.maximumPower);
    
    // Turn off the relay
    relayManager.emergencyShutdown(1);
    
    // Refetch configuration from server after emergency shutdown
    if (WiFi.isConnected()) {
      Serial.println(F("[Config] Refetching configuration after emergency shutdown"));
      updateConfiguration();
    }
  }
  // Check Socket 2 power threshold
  if (sensor2Data.isValid && sensor2Data.power > socket2Config.maximumPower && socket2Config.isOn)
  {
    Serial.print(F("[Alert] Socket 2 power exceeded threshold! "));
    Serial.print(sensor2Data.power);
    Serial.print(F("W > "));
    Serial.print(socket2Config.maximumPower);
    Serial.println(F("W"));

    // Turn off the relay
    relayManager.emergencyShutdown(2);

    // Send alert via MQTT
    mqttManager.publishAlert(2, sensor2Data.power, socket2Config.maximumPower);
    
    // Refetch configuration from server after emergency shutdown
    if (WiFi.isConnected()) {
      Serial.println(F("[Config] Refetching configuration after emergency shutdown"));
      updateConfiguration();
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
        Serial.print(F("[System] Published sensor data: Socket1="));
        Serial.print(socket1Data.power);
        Serial.print(F("W ("));
        Serial.print(socket1Data.voltage);
        Serial.print(F("V, "));
        Serial.print(socket1Data.current);
        Serial.print(F("A), Socket2="));
        Serial.print(socket2Data.power);
        Serial.print(F("W ("));
        Serial.print(socket2Data.voltage);
        Serial.print(F("V, "));
        Serial.print(socket2Data.current);
        Serial.println(F("A)"));
      }
    }
  }
}