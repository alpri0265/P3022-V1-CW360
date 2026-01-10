#ifndef MENUMANAGER_H
#define MENUMANAGER_H

#include <Arduino.h>
#include "Encoder.h"
#include "LCDDisplay.h"
#include "Settings.h"
#include "Utils.h"
#include "Config.h"

// ---------------- Menu Manager Class ----------------
class MenuManager {
public:
  enum Screen : uint8_t {
    SCR_MAIN = 0,
    SCR_MENU,
    SCR_VIEW,
    SCR_ADC,
    SCR_ZERO,
    SCR_SETVALUE,
    SCR_CALMIN,
    SCR_CALMAX,
    SCR_INVERT,
  };

  // Callback function types for settings actions
  typedef void (*SetZeroCallback)(uint16_t);
  typedef void (*SetValueCallback)(uint16_t, uint16_t);
  typedef void (*CalMinCallback)(uint16_t);
  typedef void (*CalMaxCallback)(uint16_t);
  typedef void (*InvertToggleCallback)();

  MenuManager(LCDDisplay& lcd, SetZeroCallback setZero, SetValueCallback setValue,
              CalMinCallback calMin, CalMaxCallback calMax, InvertToggleCallback invertToggle,
              Settings* settings);

  // Process UI events and update display
  // Returns: number of encoder steps actually processed (for delta consumption)
  int16_t update(uint16_t adc, uint16_t raw100, uint16_t shown100, const Encoder::State& encState);

  // Get current screen
  Screen getCurrentScreen() const { return currentScreen_; }
  
  // Get current menu index (for debug)
  uint8_t getMenuIndex() const { return menuIdx_; }
  
  // Get accumulator value (for debug)
  int16_t getAccumulator() const { return menuAccum_; }

private:
  LCDDisplay& lcd_;
  Screen currentScreen_;
  uint8_t menuIdx_;
  
  // Set Value editor state
  uint16_t target100_; // 0..35999
  uint16_t step100_;   // 1=0.01°, 10=0.1°, 100=1°, 1000=10°
  
  // Menu navigation accumulator - accumulates encoder steps, processes 1 detent (4 steps) per UI cycle
  int16_t menuAccum_;  // Accumulates delta, processes 1 detent (4 steps) per UI cycle for smooth navigation
  
  // Menu items
  static const uint8_t MENU_N = 7;
  const char* menuItems_[MENU_N];
  
  // Callbacks for settings actions
  SetZeroCallback setZero_;
  SetValueCallback setValue_;
  CalMinCallback calMin_;
  CalMaxCallback calMax_;
  InvertToggleCallback invertToggle_;
  Settings* settings_;

  // Clamp value to range [lo, hi]
  static inline int16_t clampi16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
  }

  // Process encoder events and update state machine
  // Process events, returns number of encoder steps actually processed
  int16_t processEvents(const Encoder::State& encState, uint16_t adc, uint16_t raw100, uint16_t shown100);

  // Render current screen to LCD
  void render(uint16_t adc, uint16_t raw100, uint16_t shown100);
};

#endif // MENUMANAGER_H
