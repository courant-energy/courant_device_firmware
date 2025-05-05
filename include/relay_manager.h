#ifndef RELAY_MANAGER_H
#define RELAY_MANAGER_H

#include <Arduino.h>
#include "config.h"

class RelayManager {
private:
    bool relay1State;
    bool relay2State;
    
public:
    RelayManager();
    
    void begin();
    
    // Control methods
    void setRelay1(bool state);
    void setRelay2(bool state);
    
    bool getRelay1State() { return relay1State; }
    bool getRelay2State() { return relay2State; }
    
    // Turn off relays (safety function)
    void emergencyShutdown(uint8_t relayNumber);
};

#endif // RELAY_MANAGER_H