#include "../include/sensor_manager.h"

SensorManager::SensorManager() {
  pzemSerial1 = nullptr;
  pzemSerial2 = nullptr;
  pzem1 = nullptr;
  pzem2 = nullptr;
  lastReadTime = 0;
  
  // Initialize sensor data with invalid values
  sensor1Data.isValid = false;
  sensor2Data.isValid = false;
}

SensorManager::~SensorManager() {
  if (pzem1) delete pzem1;
  if (pzem2) delete pzem2;
  if (pzemSerial1) delete pzemSerial1;
  if (pzemSerial2) delete pzemSerial2;
}

bool SensorManager::begin() {
  if (DEBUG_ENABLED) {
    Serial.println("[Sensor] Initializing PZEM sensors");
  }
  
  // Initialize SoftwareSerial for PZEM1
  pzemSerial1 = new SoftwareSerial(PZEM1_RX_PIN, PZEM1_TX_PIN);
  pzem1 = new PZEM004Tv30(*pzemSerial1);
  
  // Initialize SoftwareSerial for PZEM2
  pzemSerial2 = new SoftwareSerial(PZEM2_RX_PIN, PZEM2_TX_PIN);
  pzem2 = new PZEM004Tv30(*pzemSerial2);
  
  // Reset PZEM energy counters (optional)
  pzem1->resetEnergy();
  pzem2->resetEnergy();
  
  if (DEBUG_ENABLED) {
    Serial.println("[Sensor] PZEM sensors initialized");
  }
  
  return true;
}

bool SensorManager::update() {
  unsigned long currentTime = millis();
  
  // Check if it's time to read sensors (based on SENSOR_READING_INTERVAL)
  if (currentTime - lastReadTime >= SENSOR_READING_INTERVAL) {
    lastReadTime = currentTime;
    
    // Read from both sensors
    bool sensor1Success = readSensor(pzem1, sensor1Data);
    bool sensor2Success = readSensor(pzem2, sensor2Data);
    
    if (DEBUG_ENABLED) {
      if (sensor1Success) {
        Serial.println("[Sensor] PZEM1 Reading: " + 
                      String(sensor1Data.voltage) + "V, " + 
                      String(sensor1Data.current, 3) + "A, " + 
                      String(sensor1Data.power) + "W, PF: " + 
                      String(sensor1Data.powerFactor));
      } else {
        Serial.println("[Sensor] Failed to read from PZEM1");
      }
      
      if (sensor2Success) {
        Serial.println("[Sensor] PZEM2 Reading: " + 
                      String(sensor2Data.voltage) + "V, " + 
                      String(sensor2Data.current, 3) + "A, " + 
                      String(sensor2Data.power) + "W, PF: " + 
                      String(sensor2Data.powerFactor));
      } else {
        Serial.println("[Sensor] Failed to read from PZEM2");
      }
    }
    
    return sensor1Success || sensor2Success;
  }
  
  return false;
}

bool SensorManager::readSensor(PZEM004Tv30* pzem, SensorData& data) {
  // Read values with error handling
  float voltage = pzem->voltage();
  if (isnan(voltage)) {
    data.isValid = false;
    return false;
  }
  
  float current = pzem->current();
  if (isnan(current)) {
    data.isValid = false;
    return false;
  }
  
  float power = pzem->power();
  if (isnan(power)) {
    data.isValid = false;
    return false;
  }
  
  float energy = pzem->energy();
  float frequency = pzem->frequency();
  float pf = pzem->pf();
  
  // Fill in data structure
  data.voltage = voltage;
  data.current = current;
  data.power = power;
  data.energy = energy;
  data.frequency = frequency;
  data.powerFactor = pf;
  data.isValid = true;
  
  return true;
}