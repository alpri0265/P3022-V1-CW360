#include "Encoder.h"

Encoder::Encoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW) 
  : pinA_(pinA), pinB_(pinB), pinSW_(pinSW),
    prevAB_(0), lastAB_(0), abCount_(0),
    swPrevUp_(true), swLastUp_(true), swLastChangeMs_(0),
    swDownMs_(0), longFired_(false) {
  // State is initialized by default constructor
}

void Encoder::begin() {
  pinMode(pinA_, INPUT_PULLUP);
  pinMode(pinB_, INPUT_PULLUP);
  pinMode(pinSW_, INPUT_PULLUP);
  
  // Read initial state to avoid false first movement
  uint8_t a = digitalRead(pinA_) & 1;
  uint8_t b = digitalRead(pinB_) & 1;
  prevAB_ = (a << 1) | b;
  lastAB_ = prevAB_;
  abCount_ = 0;
  
  swPrevUp_ = digitalRead(pinSW_);
  swLastUp_ = swPrevUp_;
  swLastChangeMs_ = millis();
}

void Encoder::update() {
  static const uint16_t SW_DEBOUNCE_MS = 25;      // Button debounce time
  static const uint8_t AB_STABLE_TICKS = 2;       // Encoder AB lines must be stable for 2 ticks
  
  // A/B decode (quadrature encoder with debouncing)
  uint8_t a = digitalRead(pinA_);
  uint8_t b = digitalRead(pinB_);
  uint8_t ab = (a << 1) | b;

  // Debounce encoder lines: require stable state for multiple ticks
  if (ab == lastAB_) {
    if (abCount_ < AB_STABLE_TICKS) abCount_++;
  } else {
    abCount_ = 0;
    lastAB_ = ab;
  }

  // Process encoder rotation only when state is stable and changed
  if (abCount_ >= AB_STABLE_TICKS && ab != prevAB_) {
    // Lookup table for quadrature decoding (Gray code sequence)
    // Index: (prevAB << 2) | currentAB (4 bits)
    static const int8_t abTable[16] = {
      0,  1, -1,  0,  // prev=00: 00->00=0, 00->01=+1, 00->10=-1, 00->11=0
     -1,  0,  0,  1,  // prev=01: 01->00=-1, 01->01=0, 01->10=0, 01->11=+1
      1,  0,  0, -1,  // prev=10: 10->00=+1, 10->01=0, 10->10=0, 10->11=-1
      0, -1,  1,  0   // prev=11: 11->00=0, 11->01=-1, 11->10=+1, 11->11=0
    };
    uint8_t idx = (prevAB_ << 2) | ab;
    int8_t step = abTable[idx];
    if (step != 0) state_.delta += step;
    prevAB_ = ab;
  }

  // Button handling with debouncing and long press detection
  bool up = digitalRead(pinSW_); // true=not pressed (pullup), false=pressed
  uint32_t now = millis();

  // Track button state changes for debouncing
  if (up != swLastUp_) {
    swLastUp_ = up;
    swLastChangeMs_ = now;
  }

  // Apply debouncing: only accept state change after debounce period
  if ((uint32_t)(now - swLastChangeMs_) >= SW_DEBOUNCE_MS && up != swPrevUp_) {
    swPrevUp_ = up;
    if (!swPrevUp_) { // Button pressed
      swDownMs_ = now;
      longFired_ = false;
    } else { // Button released (after debounce)
      if (!longFired_) state_.click = true; // Only fire click if long press wasn't triggered
    }
  }
  
  // Long press detection: >600ms hold
  if (!swPrevUp_ && !longFired_ && (uint32_t)(now - swDownMs_) > 600) {
    state_.longClick = true;
    longFired_ = true; // Prevent click event when released
  }
}

Encoder::State Encoder::getState(bool resetFlags) {
  State s = state_;
  if (resetFlags) {
    state_.delta = 0;
    state_.click = false;
    state_.longClick = false;
  }
  return s;
}
