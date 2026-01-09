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
#define LCD_INTERFACE_I2C
// #define LCD_INTERFACE_PARALLEL_4BIT

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
#define LCD_I2C_ADDR 0x27  // Change to 0x3F if needed

// Pin definitions for 4-bit parallel interface
#define PIN_LCD_RS 12
#define PIN_LCD_EN 11
#define PIN_LCD_D4 7
#define PIN_LCD_D5 6
#define PIN_LCD_D6 5
#define PIN_LCD_D7 8

// ---------------- Pin Definitions ----------------
// These pins are compatible across Uno, Nano, and Micro
static const uint8_t PIN_ENC_A  = 2;   // Encoder channel A (interrupt-capable on most boards)
static const uint8_t PIN_ENC_B  = 3;   // Encoder channel B (interrupt-capable on most boards)
static const uint8_t PIN_ENC_SW = 4;   // Encoder switch/button

static const uint8_t PIN_ANGLE  = A0;  // Analog input for P3022 sensor

// ---------------- Timing Constants ----------------
static const uint16_t ENCODER_TICK_MS = 2;   // Encoder polling: 2ms
static const uint16_t UI_TICK_MS = 20;       // UI update: 20ms = 50Hz

#endif // CONFIG_H
