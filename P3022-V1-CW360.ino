/*
  Arduino Uno/Nano/Micro Compatible
  P3022 Angle Sensor with LCD Display and Rotary Encoder Menu
  
  Compatible boards:
  - Arduino Uno (ATmega328P) - Full support
  - Arduino Nano (ATmega328P) - Full support  
  - Arduino Micro (ATmega32U4) - Full support
  
  Hardware:
  - LCD Display (1602 or 2004) via I2C or 4-bit parallel
  - EC-11 Rotary Encoder (A/B + SW)
  - P3022-V1-CW360 analog angle sensor

  Features:
  - Reads P3022 analog output on A0 (ADC averaged for stability)
  - Calibration MIN/MAX (stores to EEPROM with CRC validation)
  - Zero offset (Set Zero) stores to EEPROM
  - Set Value: set displayed angle to arbitrary target (e.g. 70.42°) by adjusting zero offset
  - Invert direction (stores to EEPROM)
  - Menu controlled by EC-11 encoder:
      Rotate: navigate / edit value
      Click: OK / change step in Set Value
      Long press: Back (or Apply in Set Value); On main screen long press = quick Set Zero
  - LCD redraw without frequent lcd.clear() to reduce flicker
  - Optimized polling rates for stable operation

  Libraries required:
  - For I2C: LiquidCrystal_I2C (by Frank de Brabander or similar)
  - For 4-bit: LiquidCrystal (built-in Arduino library)
  - EEPROM (built-in Arduino library)
  - Wire (built-in Arduino library, only for I2C mode)

  LCD Configuration (set below):
  - LCD_TYPE: 1602 (16x2) or 2004 (20x4)
  - LCD_INTERFACE: I2C or PARALLEL_4BIT
  
  Wiring I2C:
    - Uno/Nano: SDA=A4, SCL=A5
    - Micro: SDA=D2, SCL=D3 (handled automatically by Wire library)
    - VCC=5V, GND=GND
    - I2C address: 0x27 or 0x3F (set LCD_I2C_ADDR below)
  
  Wiring 4-bit Parallel (only if LCD_INTERFACE == PARALLEL_4BIT):
    - RS=12, Enable=11, D4=7, D5=6, D6=5, D7=8
    - RW=GND (read/write always low)
    - VCC=5V, GND=GND, V0=potentiometer (contrast)
    - Can customize pins by changing PIN_LCD_* below
  
  P3022 Sensor:
    - OUT=A0 (analog input)
    - VCC=5V, GND=GND
  
  EC-11 Encoder:
    - A=D2, B=D3, SW=D4 (button to GND)
    - Internal INPUT_PULLUP enabled (no external resistors needed)

  Notes:
  - Code automatically adapts to board type (detected at compile time)
  - All settings are stored in EEPROM with CRC protection
  - Works with 16MHz Uno/Nano and 16MHz Micro (both 5V)
*/

// Include configuration first (defines LCD_TYPE, LCD_INTERFACE, etc.)
#include "Config.h"

// Include LCD library based on interface type (must be before LCDDisplay.h)
#if defined(LCD_INTERFACE_I2C)
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
  // Global LCD object (will be used by LCDDisplay class)
  LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
#elif defined(LCD_INTERFACE_PARALLEL_4BIT)
  #include <LiquidCrystal.h>
  // Global LCD object (will be used by LCDDisplay class)
  LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
#endif

// Include all module headers (order matters for dependencies)
#include "Settings.h"
#include "Sensor.h"
#include "Encoder.h"
#include "LCDDisplay.h"  // Requires lcd object defined above
#include "Utils.h"
#include "MenuManager.h"  // Requires LCDDisplay and Utils
#include <string.h>  // For memcpy in LCDDisplay

// ---------------- Global Instances ----------------
// Global encoder instance
Encoder encoder(PIN_ENC_A, PIN_ENC_B, PIN_ENC_SW);

// Global LCD display instance (references global lcd object)
LCDDisplay lcdDisplay(lcd);

// Global menu manager instance (will be initialized in setup)
MenuManager* menuManager = nullptr;

// Callback functions for menu actions (wrappers for settings functions)
void menuSetZero(uint16_t raw100) {
  doSetZero(raw100);
}

void menuSetValue(uint16_t raw100, uint16_t target100) {
  doSetValue(raw100, target100);
}

void menuCalMin(uint16_t adc) {
  doCalMin(adc);
}

void menuCalMax(uint16_t adc) {
  doCalMax(adc);
}

void menuInvertToggle() {
  doInvertToggle();
}

       // ---------------- Timing Variables ----------------
       // Timing constants are defined in Config.h
       // Note: Encoder rotation is now handled by interrupts, so encoder.update() 
       // only needs to be called for button processing (can be slower)
       uint32_t lastEncTick = 0;
       uint32_t lastUiTick  = 0;

void setup() {
  #ifdef DEBUG_ENCODER_DELTA
  Serial.begin(115200);
  // For Arduino Micro: Serial works immediately, no need to wait
  // For other boards: small delay to ensure Serial is ready
  delay(500);
  Serial.println("=== Encoder Debug Mode ===");
  Serial.print("ENCODER_STEPS_PER_DETENT = ");
  Serial.println(ENCODER_STEPS_PER_DETENT);
  Serial.println("Rotate encoder to see delta values...");
  Serial.println();
  #endif

  // Configure ADC reference
  // DEFAULT = AVcc (5V for Uno/Nano/Micro)
  // For 3.3V boards, use INTERNAL or EXTERNAL
  analogReference(DEFAULT);

  // Load settings from EEPROM (or defaults if first run)
  loadSettings();

  // Initialize encoder (configures pins and reads initial state)
  encoder.begin();

  // Initialize LCD display and show startup message
  lcdDisplay.showStartup();

  // Initialize menu manager
  static MenuManager menu(lcdDisplay, menuSetZero, menuSetValue, 
                          menuCalMin, menuCalMax, menuInvertToggle, &S);
  menuManager = &menu;
}

void loop() {
         uint32_t now = millis();

         // Encoder tick - button processing only (rotation is handled by interrupts)
         // Can be called less frequently now (10ms is enough for button debouncing)
         if ((uint32_t)(now - lastEncTick) >= ENCODER_TICK_MS) {
           lastEncTick = now;
           encoder.update(); // Process button state (rotation handled by ISR)
         }

  // UI tick (20ms = 50Hz update rate for smooth display)
  if ((uint32_t)(now - lastUiTick) >= UI_TICK_MS) {
    lastUiTick = now;

    uint16_t adc    = readAdcAvg16();           // Read averaged ADC value (0..1023)
    uint16_t raw100 = adcToAngle100(adc);       // Convert to angle (0..35999, calibrated, invert applied, no zero offset)
    uint16_t shown  = applyZero100(raw100);     // Apply zero offset to get displayed angle

    // Update menu with encoder events and sensor data
    if (menuManager) {
      // Get encoder state without resetting (we'll consume delta partially)
      Encoder::State encState = encoder.getState(false);
      
      #ifdef DEBUG_ENCODER_DELTA
      // Print debug info BEFORE menu update to see original delta and accumulator
      static uint8_t lastMenuIdxBefore = 255;
      static int16_t lastAccumBefore = 999;
      if (menuManager && menuManager->getCurrentScreen() == MenuManager::SCR_MENU) {
        uint8_t menuIdxBefore = menuManager->getMenuIndex();
        int16_t accumBefore = menuManager->getAccumulator();
        if (encState.delta != 0 || menuIdxBefore != lastMenuIdxBefore || accumBefore != lastAccumBefore) {
          Serial.print("[BEFORE] Delta: ");
          Serial.print(encState.delta);
          Serial.print(" | MenuIdx: ");
          Serial.print(menuIdxBefore);
          Serial.print(" | Accum: ");
          Serial.print(accumBefore);
          Serial.print(" | STEPS/DETENT=");
          Serial.println(ENCODER_STEPS_PER_DETENT);
          lastMenuIdxBefore = menuIdxBefore;
          lastAccumBefore = accumBefore;
        }
      }
      #endif
      
      // Update menu and get number of encoder steps actually processed
      int16_t processedSteps = menuManager->update(adc, raw100, shown, encState);
      
      #ifdef DEBUG_ENCODER_DELTA
      // Print debug info AFTER menu update to see what changed
      if (menuManager && menuManager->getCurrentScreen() == MenuManager::SCR_MENU) {
        static uint8_t lastMenuIdxAfter = 255;
        static int16_t lastAccumAfter = 999;
        uint8_t menuIdxAfter = menuManager->getMenuIndex();
        int16_t accumAfter = menuManager->getAccumulator();
        if (processedSteps != 0 || menuIdxAfter != lastMenuIdxAfter || accumAfter != lastAccumAfter) {
          Serial.print("[AFTER]  Processed: ");
          Serial.print(processedSteps);
          Serial.print(" | MenuIdx: ");
          Serial.print(menuIdxAfter);
          Serial.print(" | Accum: ");
          Serial.println(accumAfter);
          lastMenuIdxAfter = menuIdxAfter;
          lastAccumAfter = accumAfter;
        }
      }
      #endif
      
      // Consume only the steps that were actually processed by MenuManager
      // For menu: MenuManager processes 1 detent (ENCODER_STEPS_PER_DETENT) if available
      // For other screens: processes all delta
      if (processedSteps != 0) {
        encoder.consumeDelta(processedSteps);
      }
      // If processedSteps == 0 (partial detent in menu), delta remains in encoder for next cycle
      
      // Reset button flags
      if (encState.click || encState.longClick) {
        encoder.resetButtonFlags();
      }
    }
  }
}
