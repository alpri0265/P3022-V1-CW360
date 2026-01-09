#include "MenuManager.h"

MenuManager::MenuManager(LCDDisplay& lcd, SetZeroCallback setZero, SetValueCallback setValue,
                         CalMinCallback calMin, CalMaxCallback calMax, InvertToggleCallback invertToggle,
                         Settings* settings)
  : lcd_(lcd), currentScreen_(SCR_MAIN), menuIdx_(0), target100_(0), step100_(1),
    setZero_(setZero), setValue_(setValue), calMin_(calMin), calMax_(calMax), 
    invertToggle_(invertToggle), settings_(settings) {
  
  // Initialize menu items
  menuItems_[0] = "View";
  menuItems_[1] = "Set Zero";
  menuItems_[2] = "Set Value";
  menuItems_[3] = "Cal Min";
  menuItems_[4] = "Cal Max";
  menuItems_[5] = "Invert";
}

void MenuManager::update(uint16_t adc, uint16_t raw100, uint16_t shown100, const Encoder::State& encState) {
  // Process state machine with encoder events
  processEvents(encState, adc, raw100, shown100);
  
  // Render current screen
  render(adc, raw100, shown100);
}

void MenuManager::processEvents(const Encoder::State& encState, uint16_t adc, uint16_t raw100, uint16_t shown100) {
  int16_t d = encState.delta;
  bool click = encState.click;
  bool longClick = encState.longClick;

  // State machine
  if (currentScreen_ == SCR_MAIN) {
    if (click) currentScreen_ = SCR_MENU;
    if (longClick && setZero_) {
      setZero_(raw100); // Quick set zero from main
    }
  }
  else if (currentScreen_ == SCR_MENU) {
    if (d) {
      int16_t ni = (int16_t)menuIdx_ + (d > 0 ? 1 : -1);
      ni = clampi16(ni, 0, (int16_t)MENU_N - 1);
      menuIdx_ = (uint8_t)ni;
    }
    if (click) {
      switch (menuIdx_) {
        case 0: currentScreen_ = SCR_VIEW; break;
        case 1: currentScreen_ = SCR_ZERO; break;
        case 2:
          currentScreen_ = SCR_SETVALUE;
          target100_ = shown100; // Start editing from current shown value
          step100_ = 1;          // 0.01°
          break;
        case 3: currentScreen_ = SCR_CALMIN; break;
        case 4: currentScreen_ = SCR_CALMAX; break;
        case 5: currentScreen_ = SCR_INVERT; break;
      }
    }
    if (longClick) currentScreen_ = SCR_MAIN;
  }
  else if (currentScreen_ == SCR_SETVALUE) {
    // Rotate encoder => change target angle value
    if (d) {
      int32_t t = (int32_t)target100_ + (d > 0 ? (int32_t)step100_ : -(int32_t)step100_);
      // Wrap around to keep in 0..35999 range
      if (t < 0) t += 36000;
      else if (t >= 36000) t -= 36000;
      target100_ = (uint16_t)t;
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
        snprintf(buf0, LCD_COLS + 1, "ABS:%s deg", a);
        snprintf(buf1, LCD_COLS + 1, "Click:MENU L:0");
        #if LCD_ROWS >= 4
          snprintf(buf2, LCD_COLS + 1, "");
          snprintf(buf3, LCD_COLS + 1, "");
        #endif
      }
      break;

    case SCR_MENU:
      snprintf(buf0, LCD_COLS + 1, ">%s", menuItems_[menuIdx_]);
      snprintf(buf1, LCD_COLS + 1, "Click:OK L:Back");
      #if LCD_ROWS >= 4
        if (menuIdx_ > 0) {
          snprintf(buf2, LCD_COLS + 1, "  %s", menuItems_[menuIdx_ - 1]);
        } else {
          snprintf(buf2, LCD_COLS + 1, "");
        }
        if (menuIdx_ < MENU_N - 1) {
          snprintf(buf3, LCD_COLS + 1, "  %s", menuItems_[menuIdx_ + 1]);
        } else {
          snprintf(buf3, LCD_COLS + 1, "");
        }
      #endif
      break;

    case SCR_VIEW:
      {
        char a[8]; formatAngle100(a, shown100);
        snprintf(buf0, LCD_COLS + 1, "Angle:%s deg", a);
        snprintf(buf1, LCD_COLS + 1, "ADC:%4u", adc);
        #if LCD_ROWS >= 4
          char raw[8]; formatAngle100(raw, raw100);
          snprintf(buf2, LCD_COLS + 1, "Raw: %s", raw);
          if (settings_) {
            snprintf(buf3, LCD_COLS + 1, "Zero:%5u", settings_->zero100);
          } else {
            snprintf(buf3, LCD_COLS + 1, "");
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
        snprintf(buf0, LCD_COLS + 1, "Set:%s deg", t);
        if (step100_ == 1)        snprintf(buf1, LCD_COLS + 1, "Step:0.01 L:OK");
        else if (step100_ == 10)  snprintf(buf1, LCD_COLS + 1, "Step:0.1  L:OK");
        else if (step100_ == 100) snprintf(buf1, LCD_COLS + 1, "Step:1    L:OK");
        else                     snprintf(buf1, LCD_COLS + 1, "Step:10   L:OK");
        #if LCD_ROWS >= 4
          char a[8]; formatAngle100(a, raw100);
          snprintf(buf2, LCD_COLS + 1, "Raw: %s", a);
          snprintf(buf3, LCD_COLS + 1, "Rotate:change");
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
