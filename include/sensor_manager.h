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

struct PowerSample {
    float values[MAX_SAMPLES];
    int count;
    int index;
    
    PowerSample() : count(0), index(0) {
        for (int i = 0; i < MAX_SAMPLES; i++) {
            values[i] = 0;
        }
    }
    
    void addSample(float value) {
        values[index] = value;
        index = (index + 1) % MAX_SAMPLES;
        if (count < MAX_SAMPLES) count++;
    }
    
    float getAverage() {
        if (count == 0) return 0;
        float sum = 0;
        for (int i = 0; i < count; i++) {
            sum += values[i];
        }
        return sum / count;
    }
    
    void reset() {
        count = 0;
        index = 0;
        for (int i = 0; i < MAX_SAMPLES; i++) {
            values[i] = 0;
        }
    }
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
    
    PowerSample sensor1PowerSamples;
    PowerSample sensor2PowerSamples;
    
    bool readSensor(PZEM004Tv30* pzem, SensorData& data);
    
public:
    SensorManager();
    ~SensorManager();
    
    bool begin();
    bool update();
    SensorData getSensor1Data() { return sensor1Data; }
    SensorData getSensor2Data() { return sensor2Data; }
    float getSensor1AveragePower() { return sensor1PowerSamples.getAverage(); }
    float getSensor2AveragePower() { return sensor2PowerSamples.getAverage(); }
    void resetAverages();
};

#endif // SENSOR_MANAGER_H