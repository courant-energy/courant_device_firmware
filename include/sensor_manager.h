#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

#include <Arduino.h>
#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>
#include "config.h"

struct SensorData {
    float voltage;       // V
    float current;       // A
    float power;         // W
    float energy;        // kWh
    float frequency;     // Hz
    float powerFactor;   
    bool isValid;
};

class SensorManager {
private:
    SoftwareSerial *pzemSerial1;
    SoftwareSerial *pzemSerial2;
    PZEM004Tv30 *pzem1;
    PZEM004Tv30 *pzem2;
    unsigned long lastReadTime;
    
    SensorData sensor1Data;
    SensorData sensor2Data;
    
    bool readSensor(PZEM004Tv30* pzem, SensorData& data);
    
public:
    SensorManager();
    ~SensorManager();
    
    bool begin();
    bool update();
    SensorData getSensor1Data() { return sensor1Data; }
    SensorData getSensor2Data() { return sensor2Data; }
};

#endif // SENSOR_MANAGER_H