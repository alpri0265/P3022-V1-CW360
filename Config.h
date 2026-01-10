#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------------- Board Detection ----------------
// Auto-detect board type for compatibility
#if defined(__AVR_ATmega32U4__)
  #define BOARD_TYPE "Micro (32U4)"
#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)
  #define BOARD_TYPE "Uno/Nano (328P)"
#else
  #define BOARD_TYPE "Unknown"
#endif

// ---------------- LCD Configuration ----------------
// Choose LCD type: 1602 (16x2) or 2004 (20x4)
#define LCD_TYPE_1602
// #define LCD_TYPE_2004

// Choose interface: I2C or PARALLEL_4BIT
// #define LCD_INTERFACE_I2C
#define LCD_INTERFACE_PARALLEL_4BIT

#if defined(LCD_TYPE_1602)
  #define LCD_COLS 16
  #define LCD_ROWS 2
#elif defined(LCD_TYPE_2004)
  #define LCD_COLS 20
  #define LCD_ROWS 4
#else
  #error "Please define LCD_TYPE_1602 or LCD_TYPE_2004"
#endif

// I2C address for I2C interface
//#define LCD_I2C_ADDR 0x27  // Change to 0x3F if needed

// Pin definitions for 4-bit parallel interface
#define PIN_LCD_RS 12
#define PIN_LCD_EN 11
#define PIN_LCD_D4 7
#define PIN_LCD_D5 6
#define PIN_LCD_D6 5
#define PIN_LCD_D7 8

// ---------------- Pin Definitions ----------------
// These pins are compatible across Uno, Nano, and Micro
// IMPORTANT: PIN_ENC_A and PIN_ENC_B MUST be interrupt-capable pins!
// - Uno/Nano: Pin 2 = INT0, Pin 3 = INT1
// - Micro: Pin 3 = INT0, Pin 2 = INT1 (digitalPinToInterrupt() handles this automatically)
// Encoder rotation is now handled by external interrupts for precise, real-time detection
static const uint8_t PIN_ENC_A  = 2;   // Encoder channel A (MUST be interrupt-capable: INT0/INT1)
static const uint8_t PIN_ENC_B  = 3;   // Encoder channel B (MUST be interrupt-capable: INT0/INT1)
static const uint8_t PIN_ENC_SW = 4;   // Encoder switch/button (polling, no interrupt needed)

static const uint8_t PIN_ANGLE  = A0;  // Analog input for P3022 sensor

// ---------------- Timing Constants ----------------
// Note: Encoder rotation is now handled by interrupts, so ENCODER_TICK_MS is only
// used for button processing (debouncing). Can be increased to 10ms without issues.
static const uint16_t ENCODER_TICK_MS = 10;  // Button processing: 10ms (rotation handled by ISR)
static const uint16_t UI_TICK_MS = 10;       // UI update: 10ms = 100Hz

// ---------------- Encoder Configuration ----------------
// Number of encoder pulses (steps) per physical detent click
// With interrupt-based encoder, we capture ALL quadrature transitions, so this value
// represents how many steps are needed to move menu by 1 item.
// 
// Goal: 1 physical click = 1 menu item movement
// If encoder generates ~4 pulses per physical click (typical EC-11 encoder),
// set ENCODER_STEPS_PER_DETENT = 4 for 1:1 mapping.
//
// Common values:
//   - 1: Extremely sensitive (every step moves menu - may cause skipping)
//   - 2: Very responsive (2 steps = 1 menu item, ~0.5 clicks per item)
//   - 4: Standard for EC-11 (4 steps = 1 menu item, ~1 click per item)
//   - 8: Less responsive (8 steps = 1 menu item, ~2 clicks per item)
//
// Adjust this value based on your encoder behavior and desired responsiveness
// Typical EC-11 encoder generates 4 quadrature pulses per physical click.
// To achieve 1 click = 1 menu item, set ENCODER_STEPS_PER_DETENT = 4.
// 
// If menu is skipping items or too sensitive:
//   - Increase to 4 (1 click = 1 item for typical EC-11)
//   - Increase to 8 (2 clicks = 1 item, less sensitive)
// If menu is not responsive enough:
//   - Decrease to 2 (0.5 clicks = 1 item, more sensitive)
//   - Decrease to 1 (every step = 1 item, very sensitive, may skip)
// Current setting: 4 (1 click = 1 menu item for typical EC-11 encoder)
static const uint8_t ENCODER_STEPS_PER_DETENT = 4;

// ---------------- Debug Configuration ----------------
// Uncomment the line below to enable Serial debug output for encoder delta
// This will show delta values in Serial Monitor (115200 baud) to help diagnose encoder issues
// Debug mode is now disabled - encoder works correctly with interrupt-based processing
// #define DEBUG_ENCODER_DELTA

#endif // CONFIG_H
