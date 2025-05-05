#include "../include/wifi_manager.h"

WiFiManager::WiFiManager() {
  isConfigMode = false;
  isConnected = false;
  lastReconnectAttempt = 0;
  server = nullptr;
}

WiFiManager::~WiFiManager() {
  if (server) {
    server->end();
    delete server;
  }
}

bool WiFiManager::begin() {
  if (DEBUG_ENABLED) {
    Serial.println("[WiFi] Initializing WiFi Manager");
  }
  
  // Try to load WiFi configuration
  if (loadWifiConfig()) {
    // Connect to WiFi using stored credentials
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), password.c_str());
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startTime < 15000) {
      delay(500);
      if (DEBUG_ENABLED) {
        Serial.print(".");
      }
    }
    
    if (WiFi.status() == WL_CONNECTED) {
      isConnected = true;
      isConfigMode = false;
      if (DEBUG_ENABLED) {
        Serial.println();
        Serial.println("[WiFi] Connected successfully");
        Serial.print("IP: ");
        Serial.println(WiFi.localIP());
      }
      return true;
    } else {
      if (DEBUG_ENABLED) {
        Serial.println();
        Serial.println("[WiFi] Failed to connect with stored credentials");
      }
    }
  }
  
  // If we reach here, either no stored credentials or connection failed
  startAccessPoint();
  return false;
}

void WiFiManager::handleConnection() {
  if (isConfigMode) {
    // In config mode, handle the server
    return;
  }
  
  // Check if WiFi is still connected
  if (WiFi.status() != WL_CONNECTED) {
    isConnected = false;
    
    // Try to reconnect if it's time
    unsigned long now = millis();
    if (now - lastReconnectAttempt > 30000) {
      lastReconnectAttempt = now;
      if (DEBUG_ENABLED) {
        Serial.println("[WiFi] Attempting reconnection...");
      }
      
      WiFi.reconnect();
      
      // Wait briefly for connection
      unsigned long startTime = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - startTime < 5000) {
        delay(100);
      }
      
      if (WiFi.status() == WL_CONNECTED) {
        isConnected = true;
        if (DEBUG_ENABLED) {
          Serial.println("[WiFi] Reconnected successfully");
        }
      }
    }
  } else {
    isConnected = true;
  }
}

void WiFiManager::startAccessPoint() {
  if (DEBUG_ENABLED) {
    Serial.println("[WiFi] Starting Access Point for configuration");
  }
  
  // Generate unique AP name using MAC address
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  String apName = String(AP_NAME) + mac.substring(8);
  
  // Start AP mode
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apName.c_str(), AP_PASSWORD);
  
  if (DEBUG_ENABLED) {
    Serial.println("[WiFi] Access Point started");
    Serial.print("SSID: ");
    Serial.println(apName);
    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);
    Serial.print("IP Address: ");
    Serial.println(WiFi.softAPIP());
  }
  
  isConfigMode = true;
  setupConfigPortal();
}

void WiFiManager::setupConfigPortal() {
  // Create server if it doesn't exist
  if (!server) {
    server = new AsyncWebServer(80);
  }
  
  // Serve a simple configuration page
  server->on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    String html = "<html><head><title>WiFi Configuration</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>body{font-family:Arial;margin:20px;text-align:center;}";
    html += "input{width:100%;padding:10px;margin:10px 0;box-sizing:border-box;}";
    html += "button{background-color:#4CAF50;color:white;padding:10px 20px;border:none;cursor:pointer;}</style></head>";
    html += "<body><h1>Power Monitor WiFi Setup</h1>";
    html += "<form method='post' action='/save'>";
    html += "WiFi SSID:<br><input type='text' name='ssid'><br>";
    html += "WiFi Password:<br><input type='password' name='password'><br>";
    html += "<button type='submit'>Save</button></form></body></html>";
    request->send(200, "text/html", html);
  });
  
  // Handle form submission
  server->on("/save", HTTP_POST, [this](AsyncWebServerRequest *request) {
    String newSsid = "";
    String newPassword = "";
    
    if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
      newSsid = request->getParam("ssid", true)->value();
      newPassword = request->getParam("password", true)->value();
      
      // Save WiFi credentials
      saveWifiConfig(newSsid, newPassword);
      
      // Send success page with restart countdown
      String html = "<html><head><title>Configuration Saved</title>";
      html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
      html += "<style>body{font-family:Arial;text-align:center;margin:20px;}</style>";
      html += "<script>var count = 15; function countdown() { ";
      html += "document.getElementById('count').innerHTML = count; count--; ";
      html += "if(count < 0) { window.location.reload(); } else { setTimeout(countdown, 1000); }}";
      html += "</script></head><body>";
      html += "<h1>Configuration Saved!</h1>";
      html += "<p>The device will restart in <span id='count'>15</span> seconds.</p>";
      html += "<script>countdown();</script></body></html>";
      
      request->send(200, "text/html", html);
      
      // Schedule restart
      delay(1000);  // Short delay to allow the page to be sent
      ESP.restart();
    } else {
      String html = "<html><head><title>Error</title>";
      html += "<meta http-equiv='refresh' content='3;url=/' />";
      html += "</head><body><h1>Error</h1><p>Missing SSID or password. Redirecting...</p></body></html>";
      request->send(200, "text/html", html);
    }
  });
  
  // Start the server
  server->begin();
  
  // Set a timeout for the config portal
  unsigned long configStartTime = millis();
  while (isConfigMode && ((millis() - configStartTime) < (CONFIG_PORTAL_TIMEOUT * 1000))) {
    yield();
    delay(100);
  }
  
  // If timeout, restart device
  if (isConfigMode) {
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] Config portal timed out. Restarting...");
    }
    ESP.restart();
  }
}

bool WiFiManager::loadWifiConfig() {
  if (!LittleFS.begin()) {
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] Failed to mount file system");
    }
    return false;
  }
  
  if (!LittleFS.exists(WIFI_CONFIG_FILE)) {
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] No WiFi configuration file found");
    }
    return false;
  }
  
  File configFile = LittleFS.open(WIFI_CONFIG_FILE, "r");
  if (!configFile) {
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] Failed to open WiFi config file");
    }
    return false;
  }
  
  size_t size = configFile.size();
  std::unique_ptr<char[]> buf(new char[size]);
  configFile.readBytes(buf.get(), size);
  configFile.close();
  
  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, buf.get());
  
  if (error) {
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] Failed to parse WiFi config file");
    }
    return false;
  }
  
  ssid = doc["ssid"].as<String>();
  password = doc["password"].as<String>();
  
  if (ssid.length() == 0) {
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] SSID empty in config file");
    }
    return false;
  }
  
  if (DEBUG_ENABLED) {
    Serial.println("[WiFi] Loaded WiFi configuration");
    Serial.print("SSID: ");
    Serial.println(ssid);
  }
  
  return true;
}

void WiFiManager::saveWifiConfig(String newSsid, String newPassword) {
  if (!LittleFS.begin()) {
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] Failed to mount file system");
    }
    return;
  }
  
  JsonDocument doc;
  doc["ssid"] = newSsid;
  doc["password"] = newPassword;
  
  File configFile = LittleFS.open(WIFI_CONFIG_FILE, "w");
  if (!configFile) {
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] Failed to open WiFi config file for writing");
    }
    return;
  }
  
  serializeJson(doc, configFile);
  configFile.close();
  
  // Update current values
  ssid = newSsid;
  password = newPassword;
  
  if (DEBUG_ENABLED) {
    Serial.println("[WiFi] Saved WiFi configuration");
  }
}

void WiFiManager::resetConfig() {
  if (!LittleFS.begin()) {
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] Failed to mount file system for reset");
    }
    return;
  }
  
  if (LittleFS.exists(WIFI_CONFIG_FILE)) {
    LittleFS.remove(WIFI_CONFIG_FILE);
    if (DEBUG_ENABLED) {
      Serial.println("[WiFi] WiFi configuration removed");
    }
  }
}

String WiFiManager::getMacAddress() {
  return WiFi.macAddress();
}