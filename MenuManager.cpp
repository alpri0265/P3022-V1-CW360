#include "MenuManager.h"

MenuManager::MenuManager(LCDDisplay& lcd, SetZeroCallback setZero, SetValueCallback setValue,
                         CalMinCallback calMin, CalMaxCallback calMax, InvertToggleCallback invertToggle,
                         Settings* settings)
  : lcd_(lcd), currentScreen_(SCR_MAIN), menuIdx_(0), target100_(0), step100_(1),
    menuAccum_(0),
    setZero_(setZero), setValue_(setValue), calMin_(calMin), calMax_(calMax), 
    invertToggle_(invertToggle), settings_(settings) {
  
  // Initialize menu items
  menuItems_[0] = "View";
  menuItems_[1] = "View ADC";
  menuItems_[2] = "Set Zero";
  menuItems_[3] = "Set Value";
  menuItems_[4] = "Cal Min";
  menuItems_[5] = "Cal Max";
  menuItems_[6] = "Invert";
}

int16_t MenuManager::update(uint16_t adc, uint16_t raw100, uint16_t shown100, const Encoder::State& encState) {
  // Process state machine with encoder events
  int16_t processed = processEvents(encState, adc, raw100, shown100);
  
  // Render current screen
  render(adc, raw100, shown100);
  
  return processed;
}

int16_t MenuManager::processEvents(const Encoder::State& encState, uint16_t adc, uint16_t raw100, uint16_t shown100) {
  int16_t d = encState.delta;
  bool click = encState.click;
  bool longClick = encState.longClick;
  int16_t processed = 0;  // Track how many steps were actually processed

  // State machine
  if (currentScreen_ == SCR_MAIN) {
    if (click) {
      currentScreen_ = SCR_MENU;
      menuAccum_ = 0;  // Reset accumulator when entering menu
    }
    if (longClick && setZero_) {
      setZero_(raw100); // Quick set zero from main screen
    }
  }
  else if (currentScreen_ == SCR_MENU) {
    // Add new delta to accumulator (only if non-zero)
    if (d != 0) {
      // Simple accumulation: just add delta to accumulator
      // Only reset on significant direction change (>= 1 detent in opposite direction)
      // This prevents false resets from noise or small encoder bounces
      if ((menuAccum_ > 0 && d < 0 && abs(d) >= ENCODER_STEPS_PER_DETENT) ||
          (menuAccum_ < 0 && d > 0 && abs(d) >= ENCODER_STEPS_PER_DETENT)) {
        // Significant direction change - reset to new direction
        menuAccum_ = d;
      } else {
        // Same direction or small opposite delta (noise) - accumulate normally
        menuAccum_ += d;
      }
      
      // Allow accumulator to hold up to 3 detents to handle fast rotation
      // This prevents losing steps if UI cycle is slower than encoder rotation
      const int16_t maxAccum = ENCODER_STEPS_PER_DETENT * 3;
      if (menuAccum_ > maxAccum) {
        menuAccum_ = maxAccum;
      }
      if (menuAccum_ < -maxAccum) {
        menuAccum_ = -maxAccum;
      }
      
      // Always consume all delta from encoder (it's now in accumulator)
      // We'll process from accumulator below, consuming only what we actually process
      processed = d;  // Consume all delta from encoder
    }
    
    // Process exactly 1 detent from accumulator per UI cycle
    // This ensures smooth navigation: 1 UI cycle = max 1 menu item movement
    // Remaining delta stays in accumulator for next cycle
    // This prevents skipping menu items even if delta is large (e.g., Delta: 2)
    // Note: processed is already set to d above, so encoder delta will be fully consumed
    // The accumulator handles the actual menu movement at 1 detent per cycle
    if (menuAccum_ >= ENCODER_STEPS_PER_DETENT) {
      menuAccum_ -= ENCODER_STEPS_PER_DETENT;  // Remove 1 detent from accumulator
      int16_t ni = (int16_t)menuIdx_ + 1;
      ni = clampi16(ni, 0, (int16_t)MENU_N - 1);
      if (ni != (int16_t)menuIdx_) {
        menuIdx_ = (uint8_t)ni;
      } else {
        // Menu at end - reset accumulator to prevent accumulation
        menuAccum_ = 0;
      }
    } else if (menuAccum_ <= -(int16_t)ENCODER_STEPS_PER_DETENT) {
      menuAccum_ += ENCODER_STEPS_PER_DETENT;  // Remove 1 detent (add since accumulator is negative)
      int16_t ni = (int16_t)menuIdx_ - 1;
      ni = clampi16(ni, 0, (int16_t)MENU_N - 1);
      if (ni != (int16_t)menuIdx_) {
        menuIdx_ = (uint8_t)ni;
      } else {
        // Menu at start - reset accumulator to prevent accumulation
        menuAccum_ = 0;
      }
    }
    // If |menuAccum_| < ENCODER_STEPS_PER_DETENT, it's a partial detent - wait for more steps
    if (click) {
      switch (menuIdx_) {
        case 0: currentScreen_ = SCR_VIEW; break;
        case 1: currentScreen_ = SCR_ADC; break;
        case 2: currentScreen_ = SCR_ZERO; break;
        case 3:
          currentScreen_ = SCR_SETVALUE;
          target100_ = shown100; // Start editing from current shown value
          step100_ = 1;          // 0.01°
          break;
        case 4: currentScreen_ = SCR_CALMIN; break;
        case 5: currentScreen_ = SCR_CALMAX; break;
        case 6: currentScreen_ = SCR_INVERT; break;
      }
    }
    if (longClick) {
      currentScreen_ = SCR_MAIN;
      menuAccum_ = 0;  // Reset accumulator when leaving menu
    }
  }
  else if (currentScreen_ == SCR_SETVALUE) {
    // Rotate encoder => change target angle value
    // Use full delta value for smooth response to fast rotation
    if (d) {
      int32_t change = (int32_t)d * (int32_t)step100_;
      int32_t t = (int32_t)target100_ + change;
      // Wrap around to keep in 0..35999 range
      if (t < 0) t += 36000;
      else if (t >= 36000) t -= 36000;
      target100_ = (uint16_t)t;
      processed = d;  // Report that we processed all delta
    }
    // Click => cycle through step sizes (0.01°, 0.1°, 1°, 10°)
    if (click) {
      if (step100_ == 1) step100_ = 10;
      else if (step100_ == 10) step100_ = 100;
      else if (step100_ == 100) step100_ = 1000;
      else step100_ = 1;
    }
    // Long press => apply zero offset adjustment and return to menu
    if (longClick && setValue_) {
      setValue_(raw100, target100_);
      currentScreen_ = SCR_MENU;
    }
  }
  else if (currentScreen_ == SCR_VIEW || currentScreen_ == SCR_ADC) {
    // View screens: long=back, click=back to menu
    if (click || longClick) currentScreen_ = SCR_MENU;
  }
  else {
    // Action screens: click=do, long=back
    if (click) {
      switch (currentScreen_) {
        case SCR_ZERO:
          if (setZero_) setZero_(raw100);
          break;
        case SCR_CALMIN:
          if (calMin_) calMin_(adc);
          break;
        case SCR_CALMAX:
          if (calMax_) calMax_(adc);
          break;
        case SCR_INVERT:
          if (invertToggle_) invertToggle_();
          break;
        default:
          break;
      }
      currentScreen_ = SCR_MENU;
    }
    if (longClick) currentScreen_ = SCR_MENU;
  }
  
  return processed;  // Return number of steps processed
}

void MenuManager::render(uint16_t adc, uint16_t raw100, uint16_t shown100) {
  // Use dynamic buffer size based on LCD_COLS
  #if LCD_COLS == 16
    char buf0[17] = {0}, buf1[17] = {0};
    #if LCD_ROWS >= 4
      char buf2[17] = {0}, buf3[17] = {0};
    #endif
  #elif LCD_COLS == 20
    char buf0[21] = {0}, buf1[21] = {0};
    #if LCD_ROWS >= 4
      char buf2[21] = {0}, buf3[21] = {0};
    #endif
  #endif

  switch (currentScreen_) {
    case SCR_MAIN:
      {
        char a[8]; formatAngle100(a, shown100);
        snprintf(buf0, LCD_COLS + 1, "Angle: %s", a);
        snprintf(buf1, LCD_COLS + 1, "Click:MENU Long:0");
        #if LCD_ROWS >= 4
          snprintf(buf2, LCD_COLS + 1, "Long press: Set Zero");
          snprintf(buf3, LCD_COLS + 1, "");
        #endif
      }
      break;

    case SCR_MENU:
      #if LCD_COLS >= 20
        snprintf(buf0, LCD_COLS + 1, ">%d/%d %s", menuIdx_ + 1, MENU_N, menuItems_[menuIdx_]);
      #else
        snprintf(buf0, LCD_COLS + 1, ">%d %s", menuIdx_ + 1, menuItems_[menuIdx_]);
      #endif
      snprintf(buf1, LCD_COLS + 1, "Click:OK L:Back");
      #if LCD_ROWS >= 4
        if (menuIdx_ > 0) {
          snprintf(buf2, LCD_COLS + 1, "  %d %s", menuIdx_, menuItems_[menuIdx_ - 1]);
        } else {
          snprintf(buf2, LCD_COLS + 1, "");
        }
        if (menuIdx_ < MENU_N - 1) {
          snprintf(buf3, LCD_COLS + 1, "  %d %s", menuIdx_ + 2, menuItems_[menuIdx_ + 1]);
        } else {
          snprintf(buf3, LCD_COLS + 1, "");
        }
      #endif
      break;

    case SCR_VIEW:
      {
        char a[8]; formatAngle100(a, shown100);
        snprintf(buf0, LCD_COLS + 1, "Angle: %s", a);
        #if LCD_COLS >= 20
          snprintf(buf1, LCD_COLS + 1, "Click or Long: Back");
        #else
          snprintf(buf1, LCD_COLS + 1, "Click:Back");
        #endif
        #if LCD_ROWS >= 4
          char raw[8]; formatAngle100(raw, raw100);
          snprintf(buf2, LCD_COLS + 1, "Raw: %s", raw);
          if (settings_) {
            snprintf(buf3, LCD_COLS + 1, "Zero: %5u", settings_->zero100);
          } else {
            snprintf(buf3, LCD_COLS + 1, "");
          }
        #endif
      }
      break;

    case SCR_ADC:
      {
        snprintf(buf0, LCD_COLS + 1, "ADC: %4u", adc);
        if (settings_) {
          snprintf(buf1, LCD_COLS + 1, "Min:%u Max:%u", settings_->calMin, settings_->calMax);
        } else {
          snprintf(buf1, LCD_COLS + 1, "Range: 0-1023");
        }
        #if LCD_ROWS >= 4
          if (settings_) {
            int32_t span = (int32_t)settings_->calMax - (int32_t)settings_->calMin;
            if (span < 1) span = 1;
            snprintf(buf2, LCD_COLS + 1, "Span: %ld", (long)span);
            if (adc < settings_->calMin) {
              snprintf(buf3, LCD_COLS + 1, "Below MIN!");
            } else if (adc > settings_->calMax) {
              snprintf(buf3, LCD_COLS + 1, "Above MAX!");
            } else {
              uint8_t percent = (uint8_t)(((uint32_t)(adc - settings_->calMin) * 100UL) / (uint32_t)span);
              snprintf(buf3, LCD_COLS + 1, "In range: %u%%", percent);
            }
          } else {
            snprintf(buf2, LCD_COLS + 1, "Calibration not set");
            snprintf(buf3, LCD_COLS + 1, "Use Cal Min/Max");
          }
        #endif
      }
      break;

    case SCR_ZERO:
      snprintf(buf0, LCD_COLS + 1, "Set ZERO?");
      snprintf(buf1, LCD_COLS + 1, "Click:YES L:Back");
      #if LCD_ROWS >= 4
        char a[8]; formatAngle100(a, raw100);
        snprintf(buf2, LCD_COLS + 1, "Current: %s", a);
        snprintf(buf3, LCD_COLS + 1, "");
      #endif
      break;

    case SCR_SETVALUE:
      {
        char t[8]; formatAngle100(t, target100_);
        #if LCD_COLS >= 20
          snprintf(buf0, LCD_COLS + 1, "Set Value: %s", t);
        #else
          snprintf(buf0, LCD_COLS + 1, "Set:%s deg", t);
        #endif
        if (step100_ == 1) {
          #if LCD_COLS >= 20
            snprintf(buf1, LCD_COLS + 1, "Step: 0.01 Click:step L:OK");
          #else
            snprintf(buf1, LCD_COLS + 1, "St:0.01 C:step L");
          #endif
        }
        else if (step100_ == 10) {
          #if LCD_COLS >= 20
            snprintf(buf1, LCD_COLS + 1, "Step: 0.1  Click:step L:OK");
          #else
            snprintf(buf1, LCD_COLS + 1, "St:0.1  C:step L");
          #endif
        }
        else if (step100_ == 100) {
          #if LCD_COLS >= 20
            snprintf(buf1, LCD_COLS + 1, "Step: 1    Click:step L:OK");
          #else
            snprintf(buf1, LCD_COLS + 1, "St:1    C:step L");
          #endif
        }
        else {
          #if LCD_COLS >= 20
            snprintf(buf1, LCD_COLS + 1, "Step: 10   Click:step L:OK");
          #else
            snprintf(buf1, LCD_COLS + 1, "St:10   C:step L");
          #endif
        }
        #if LCD_ROWS >= 4
          char a[8]; formatAngle100(a, raw100);
          snprintf(buf2, LCD_COLS + 1, "Raw: %s", a);
          snprintf(buf3, LCD_COLS + 1, "Rotate:val Click:step");
        #endif
      }
      break;

    case SCR_CALMIN:
      snprintf(buf0, LCD_COLS + 1, "Cal MIN=%4u", adc);
      snprintf(buf1, LCD_COLS + 1, "Click:SAVE L:Back");
      #if LCD_ROWS >= 4
        if (settings_) {
          snprintf(buf2, LCD_COLS + 1, "Range: %u-%u", settings_->calMin, settings_->calMax);
        } else {
          snprintf(buf2, LCD_COLS + 1, "");
        }
        snprintf(buf3, LCD_COLS + 1, "");
      #endif
      break;

    case SCR_CALMAX:
      snprintf(buf0, LCD_COLS + 1, "Cal MAX=%4u", adc);
      snprintf(buf1, LCD_COLS + 1, "Click:SAVE L:Back");
      #if LCD_ROWS >= 4
        if (settings_) {
          snprintf(buf2, LCD_COLS + 1, "Range: %u-%u", settings_->calMin, settings_->calMax);
        } else {
          snprintf(buf2, LCD_COLS + 1, "");
        }
        snprintf(buf3, LCD_COLS + 1, "");
      #endif
      break;

    case SCR_INVERT:
      if (settings_) {
        snprintf(buf0, LCD_COLS + 1, "Invert: %s", (settings_->flags & 1) ? "ON " : "OFF");
        snprintf(buf1, LCD_COLS + 1, "Click:TOG L:Back");
        #if LCD_ROWS >= 4
          snprintf(buf2, LCD_COLS + 1, "Direction: %s", (settings_->flags & 1) ? "Reversed" : "Normal");
          snprintf(buf3, LCD_COLS + 1, "");
        #endif
      } else {
        snprintf(buf0, LCD_COLS + 1, "Invert: ERR");
        snprintf(buf1, LCD_COLS + 1, "");
      }
      break;
  }

  // Update LCD display
  lcd_.setLine(0, buf0);
  lcd_.setLine(1, buf1);
  #if LCD_ROWS >= 4
    lcd_.setLine(2, buf2);
    lcd_.setLine(3, buf3);
  #endif
  lcd_.flush();
}
