#include "../include/relay_manager.h"

RelayManager::RelayManager() : relay1State(false), relay2State(false) {
}

void RelayManager::begin() {
  if (DEBUG_ENABLED) {
    Serial.println("[Relay] Initializing relay control");
  }
  
  // Set relay pins as outputs
  pinMode(RELAY_PIN_1, OUTPUT);
  pinMode(RELAY_PIN_2, OUTPUT);
  
  // Initialize relays to OFF state
  digitalWrite(RELAY_PIN_1, LOW);
  digitalWrite(RELAY_PIN_2, LOW);
  
  if (DEBUG_ENABLED) {
    Serial.println("[Relay] Relay control initialized");
  }
}

void RelayManager::setRelay1(bool state) {
  // Only take action if state is changing
  if (state != relay1State) {
    digitalWrite(RELAY_PIN_1, state ? HIGH : LOW);
    relay1State = state;
    delay(500); // Debounce delay to prevent flickering
    
    if (DEBUG_ENABLED) {
      Serial.println("[Relay] Relay 1 set to " + String(state ? "ON" : "OFF"));
    }
  }
}

void RelayManager::setRelay2(bool state) {
  // Only take action if state is changing
  if (state != relay2State) {
    digitalWrite(RELAY_PIN_2, state ? HIGH : LOW);
    relay2State = state;
    delay(500); // Debounce delay to prevent flickering
    
    if (DEBUG_ENABLED) {
      Serial.println("[Relay] Relay 2 set to " + String(state ? "ON" : "OFF"));
    }
  }
}

void RelayManager::emergencyShutdown(uint8_t relayNumber) {
  if (relayNumber == 1) {
    digitalWrite(RELAY_PIN_1, LOW);
    relay1State = false;
    if (DEBUG_ENABLED) {
      Serial.println("[Relay] !!! EMERGENCY SHUTDOWN !!! Relay 1 turned OFF");
    }
  }
  else if (relayNumber == 2) {
    digitalWrite(RELAY_PIN_2, LOW);
    relay2State = false;
    if (DEBUG_ENABLED) {
      Serial.println("[Relay] !!! EMERGENCY SHUTDOWN !!! Relay 2 turned OFF");
    }
  }
}