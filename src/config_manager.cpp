#include "../include/config_manager.h"

ConfigManager::ConfigManager() : lastConfigCheck(0) {
}

void ConfigManager::begin(String deviceId) {
  deviceUuid = deviceId;
  
  if (DEBUG_ENABLED) {
    Serial.println(F("[Config] Initializing Configuration Manager"));
    Serial.print(F("Device UUID: "));
    Serial.println(deviceUuid);
  }
  
  // Try to load configuration from LittleFS
  if (!loadFromLittleFS()) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] No saved configuration, using defaults"));
    }
    // Set default values
    socket1Config.assignedId = 1;
    socket1Config.maximumPower = 200;  // Default 200W 
    socket1Config.isOn = true;         // Default ON
    
    socket2Config.assignedId = 2;
    socket2Config.maximumPower = 10;   // Default 10W
    socket2Config.isOn = true;         // Default ON
    
    // Save defaults
    saveToLittleFS();
  }
  
  if (DEBUG_ENABLED) {
    Serial.println(F("[Config] Configuration initialized"));
    Serial.print(F("Socket 1 - Max Power: "));
    Serial.print(socket1Config.maximumPower);
    Serial.print(F("W, State: "));
    Serial.println(socket1Config.isOn ? F("ON") : F("OFF"));
    Serial.print(F("Socket 2 - Max Power: "));
    Serial.print(socket2Config.maximumPower);
    Serial.print(F("W, State: "));
    Serial.println(socket2Config.isOn ? F("ON") : F("OFF"));
  }
}

void ConfigManager::update() {
  // Periodic configuration updates are now handled externally
  // to coordinate with MQTT connection management
}

bool ConfigManager::fetchConfig() {
  if (!WiFi.isConnected()) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] WiFi not connected, skipping config fetch"));
    }
    return false;
  }
  
  if (DEBUG_ENABLED) {
    Serial.println(F("[Config] Fetching configuration from API..."));
    Serial.print(F("[Config] Free heap before API call: "));
    Serial.print(ESP.getFreeHeap());
    Serial.println(F(" bytes"));
  }
  
  WiFiClientSecure client;
  client.setInsecure(); // Disable SSL certificate verification for simplicity
  HTTPClient http;
  
  String url = String(CONFIG_API_ENDPOINT) + "?device_uuid=" + deviceUuid;
  http.begin(client, url);
  http.addHeader("User-Agent", "ESP8266HTTPClient");
  http.addHeader("Accept", "application/json");
  http.addHeader("Connection", "close");
  int httpCode = http.GET();
  bool success = false;
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    success = parseConfigJson(payload);
    
    if (success) {
      if (DEBUG_ENABLED) {
        Serial.println(F("[Config] Successfully updated configuration from API"));
      }
      
      // Save the new config
      saveToLittleFS();
    } else {
      if (DEBUG_ENABLED) {
        Serial.println(F("[Config] Failed to parse API response"));
      }
    }
  } else {
    if (DEBUG_ENABLED) {
      Serial.print(F("[Config] API request failed, code: "));
      Serial.println(httpCode);
    }
  }
  
  http.end();
  return success;
}

bool ConfigManager::parseConfigJson(String jsonString) {
  StaticJsonDocument<512> doc;  // Reduced from default dynamic allocation
  DeserializationError error = deserializeJson(doc, jsonString);
  
  if (error) {
    if (DEBUG_ENABLED) {
      Serial.print(F("[Config] JSON parsing failed: "));
      Serial.println(error.c_str());
    }
    return false;
  }
  
  // Validate JSON structure
  if (!doc.containsKey("device_uuid") || !doc.containsKey("sockets")) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] Invalid JSON structure"));
    }
    return false;
  }
  
  // Verify device UUID matches
  String apiDeviceUuid = doc["device_uuid"].as<String>();
  if (apiDeviceUuid != deviceUuid) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] UUID mismatch in API response"));
    }
    return false;
  }
  
  // Process socket configurations
  JsonArray sockets = doc["sockets"].as<JsonArray>();
  
  for (JsonObject socket : sockets) {
    int assignedId = socket["assigned_id"].as<int>();
    float maximumPower = socket["maximum_power"].as<float>();
    bool isOn = socket["is_on"].as<bool>();
    
    if (assignedId == 1) {
      socket1Config.assignedId = assignedId;
      socket1Config.maximumPower = maximumPower;
      socket1Config.isOn = isOn;
      
      if (DEBUG_ENABLED) {
        Serial.print(F("[Config] Updated Socket 1 - Max: "));
        Serial.print(maximumPower);
        Serial.print(F("W, State: "));
        Serial.println(isOn ? F("ON") : F("OFF"));
      }
    } 
    else if (assignedId == 2) {
      socket2Config.assignedId = assignedId;
      socket2Config.maximumPower = maximumPower;
      socket2Config.isOn = isOn;
      
      if (DEBUG_ENABLED) {
        Serial.print(F("[Config] Updated Socket 2 - Max: "));
        Serial.print(maximumPower);
        Serial.print(F("W, State: "));
        Serial.println(isOn ? F("ON") : F("OFF"));
      }
    }
  }
  
  return true;
}

bool ConfigManager::loadFromLittleFS() {
  if (!LittleFS.begin()) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] Failed to mount file system"));
    }
    return false;
  }
  
  if (!LittleFS.exists(DEVICE_CONFIG_FILE)) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] No configuration file found"));
    }
    return false;
  }
  
  File configFile = LittleFS.open(DEVICE_CONFIG_FILE, "r");
  if (!configFile) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] Failed to open config file"));
    }
    return false;
  }
  
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  
  StaticJsonDocument<256> doc;  // Small local config file
  DeserializationError error = deserializeJson(doc, buf.get());
  
  if (error) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] Failed to parse config file"));
    }
    return false;
  }
  
  // Load Socket 1 config
  JsonObject socket1 = doc["socket1"];
  socket1Config.assignedId = socket1["assignedId"] | 1;  // Default to 1 if not found
  socket1Config.maximumPower = socket1["maximumPower"] | 200.0;
  socket1Config.isOn = socket1["isOn"] | true;
  
  // Load Socket 2 config
  JsonObject socket2 = doc["socket2"];
  socket2Config.assignedId = socket2["assignedId"] | 2;  // Default to 2 if not found
  socket2Config.maximumPower = socket2["maximumPower"] | 10.0;
  socket2Config.isOn = socket2["isOn"] | true;
  
  if (DEBUG_ENABLED) {
    Serial.println(F("[Config] Loaded configuration from LittleFS"));
  }
  
  return true;
}

bool ConfigManager::saveToLittleFS() {
  if (!LittleFS.begin()) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] Failed to mount file system"));
    }
    return false;
  }
  
  StaticJsonDocument<256> doc;  // Small local config file
  
  // Socket 1 config
  JsonObject socket1 = doc.createNestedObject("socket1");
  socket1["assignedId"] = socket1Config.assignedId;
  socket1["maximumPower"] = socket1Config.maximumPower;
  socket1["isOn"] = socket1Config.isOn;
  
  // Socket 2 config
  JsonObject socket2 = doc.createNestedObject("socket2");
  socket2["assignedId"] = socket2Config.assignedId;
  socket2["maximumPower"] = socket2Config.maximumPower;
  socket2["isOn"] = socket2Config.isOn;
  
  File configFile = LittleFS.open(DEVICE_CONFIG_FILE, "w");
  if (!configFile) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] Failed to open config file for writing"));
    }
    return false;
  }
  
  serializeJson(doc, configFile);
  configFile.close();
  
  if (DEBUG_ENABLED) {
    Serial.println(F("[Config] Configuration saved to LittleFS"));
  }
  
  return true;
}

void ConfigManager::setSocket1MaxPower(float power) {
  socket1Config.maximumPower = power;
  saveToLittleFS();
}

void ConfigManager::setSocket2MaxPower(float power) {
  socket2Config.maximumPower = power;
  saveToLittleFS();
}

void ConfigManager::resetConfig() {
  if (!LittleFS.begin()) {
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] Failed to mount file system for reset"));
    }
    return;
  }
  
  if (LittleFS.exists(DEVICE_CONFIG_FILE)) {
    LittleFS.remove(DEVICE_CONFIG_FILE);
    if (DEBUG_ENABLED) {
      Serial.println(F("[Config] Device configuration removed"));
    }
  }
  
  // Reset to defaults
  socket1Config.assignedId = 1;
  socket1Config.maximumPower = 200;
  socket1Config.isOn = true;
  
  socket2Config.assignedId = 2;
  socket2Config.maximumPower = 10;
  socket2Config.isOn = true;
}