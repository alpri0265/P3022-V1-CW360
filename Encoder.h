#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

// ---------------- Encoder Class (polling, RC-friendly) ----------------
class Encoder {
public:
  struct State {
    int16_t delta;      // Accumulated detents (+/-)
    bool click;         // Short click event
    bool longClick;     // Long press event (>600ms)
    
    State() : delta(0), click(false), longClick(false) {}
  };

  Encoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW);
  
  // Initialize encoder pins
  void begin();
  
  // Update encoder state (call periodically, e.g., every 2ms)
  void update();
  
  // Get current state (and optionally reset flags)
  State getState(bool resetFlags = true);

private:
  uint8_t pinA_, pinB_, pinSW_;
  State state_;
  
  // Encoder rotation state
  uint8_t prevAB_;
  uint8_t lastAB_;
  uint8_t abCount_;
  
  // Button state
  bool swPrevUp_;
  bool swLastUp_;
  uint32_t swLastChangeMs_;
  uint32_t swDownMs_;
  bool longFired_;
};

#endif // ENCODER_H
