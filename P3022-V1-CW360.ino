// test 1

/*
  ATmega328 + LCD1602 I2C(PCF8574) + EC-11 (A/B + SW, RC-friendly polling) + P3022-V1-CW360 (VCC/OUT/GND analog)

  Features:
  - Reads P3022 analog output on A0 (ADC averaged)
  - Calibration MIN/MAX (stores to EEPROM)
  - Zero offset (Set Zero) stores to EEPROM
  - Set Value: set displayed angle to an arbitrary target (e.g. 70.42°) by adjusting zero offset
  - Invert direction (stores to EEPROM)
  - Menu controlled by EC-11 encoder:
      Rotate: navigate / edit value
      Click: OK / change step in Set Value
      Long press: Back (or Apply in Set Value); On main screen long press = quick Set Zero
  - LCD redraw without frequent lcd.clear() to reduce flicker

  Libraries:
  - LiquidCrystal_I2C
  - EEPROM (built-in)

  Wiring:
  LCD I2C: SDA=A4, SCL=A5, VCC=5V, GND=GND
  P3022: OUT=A0, VCC=5V, GND=GND
  EC-11: A=D2, B=D3, SW=D4 (to GND), INPUT_PULLUP enabled

  Note:
  - LCD I2C address commonly 0x27 or 0x3F. Change below if needed.
*/

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <EEPROM.h>

// ---------------- LCD ----------------
LiquidCrystal_I2C lcd(0x27, 16, 2); // change to 0x3F if your module uses that

// ---------------- Pins ----------------
static const uint8_t PIN_ENC_A  = 2;
static const uint8_t PIN_ENC_B  = 3;
static const uint8_t PIN_ENC_SW = 4;

static const uint8_t PIN_ANGLE  = A0;

// ---------------- Settings in EEPROM ----------------
struct Settings {
  uint16_t zero100;  // 0..35999 (0.01°)
  uint16_t calMin;   // ADC raw min (0..1023)
  uint16_t calMax;   // ADC raw max (0..1023)
  uint8_t  flags;    // bit0 invert
  uint8_t  crc;      // simple XOR crc
};

Settings S;

uint8_t simple_crc(const Settings& s) {
  const uint8_t* p = (const uint8_t*)&s;
  uint8_t c = 0;
  for (size_t i = 0; i < sizeof(Settings) - 1; i++) c ^= p[i];
  return c;
}

void saveSettings() {
  S.crc = simple_crc(S);
  EEPROM.put(0, S);
}

void loadSettings() {
  EEPROM.get(0, S);
  bool bad =
    (S.crc != simple_crc(S)) ||
    (S.calMin >= S.calMax) ||
    (S.calMax > 1023) ||
    (S.zero100 >= 36000);

  if (bad) {
    S.zero100 = 0;
    S.calMin  = 0;
    S.calMax  = 1023;
    S.flags   = 0;
    saveSettings();
  }
}

// ---------------- ADC / angle math ----------------
uint16_t readAdcAvg16() {
  uint32_t acc = 0;
  for (uint8_t i = 0; i < 16; i++) acc += analogRead(PIN_ANGLE);
  return (uint16_t)(acc >> 4); // /16 => 0..1023
}

uint16_t adcToAngle100(uint16_t adc) {
  int32_t a = adc;

  // clamp by calibration
  if (a < (int32_t)S.calMin) a = S.calMin;
  if (a > (int32_t)S.calMax) a = S.calMax;

  int32_t span = (int32_t)S.calMax - (int32_t)S.calMin;
  if (span < 1) span = 1;

  // 0..35999 (0.01°). If calc reaches 36000, wrap to 0.
  int32_t ang100 = ((a - (int32_t)S.calMin) * 36000L) / span;
  if (ang100 >= 36000) ang100 = 0;

  // optional invert
  if (S.flags & 0x01) {
    ang100 = 36000 - ang100;
    if (ang100 >= 36000) ang100 = 0;
  }
  return (uint16_t)ang100;
}

uint16_t applyZero100(uint16_t angle100) {
  int32_t a = (int32_t)angle100 - (int32_t)S.zero100;
  while (a < 0) a += 36000;
  while (a >= 36000) a -= 36000;
  return (uint16_t)a;
}

// Set displayed value to target by adjusting zero:
// shown = raw - zero (mod 36000)  => zero = raw - target (mod 36000)
void doSetValue(uint16_t raw100, uint16_t target100) {
  int32_t z = (int32_t)raw100 - (int32_t)target100;
  while (z < 0) z += 36000;
  while (z >= 36000) z -= 36000;
  S.zero100 = (uint16_t)z;
  saveSettings();
}

void doSetZero(uint16_t raw100) {
  S.zero100 = raw100;
  saveSettings();
}

void doCalMin(uint16_t adc) {
  S.calMin = adc;
  if (S.calMin >= S.calMax) S.calMax = S.calMin + 1;
  if (S.calMax > 1023) S.calMax = 1023;
  saveSettings();
}

void doCalMax(uint16_t adc) {
  S.calMax = adc;
  if (S.calMax <= S.calMin) {
    S.calMin = (S.calMax > 0) ? (S.calMax - 1) : 0;
  }
  saveSettings();
}

void doInvertToggle() {
  S.flags ^= 0x01;
  saveSettings();
}

// ---------------- Encoder UI (polling, RC-friendly) ----------------
struct EncState {
  int16_t delta;      // accumulated detents (+/-)
  bool click;
  bool longClick;
};
EncState enc;

static uint8_t  prevAB     = 0;
static bool     swPrevUp   = true;   // pullup: true=up
static uint32_t swDownMs   = 0;
static bool     longFired  = false;

void encoderTick1ms() {
  // A/B decode (quadrature)
  uint8_t a = digitalRead(PIN_ENC_A);
  uint8_t b = digitalRead(PIN_ENC_B);
  uint8_t ab = (a << 1) | b;

  uint8_t p = prevAB;
  if (ab != p) {
    // one direction: 00->01->11->10->00
    if ((p == 0b00 && ab == 0b01) || (p == 0b01 && ab == 0b11) ||
        (p == 0b11 && ab == 0b10) || (p == 0b10 && ab == 0b00)) {
      enc.delta++;
    }
    // other direction: 00->10->11->01->00
    else if ((p == 0b00 && ab == 0b10) || (p == 0b10 && ab == 0b11) ||
             (p == 0b11 && ab == 0b01) || (p == 0b01 && ab == 0b00)) {
      enc.delta--;
    }
    prevAB = ab;
  }

  // Button with long press
  bool up = digitalRead(PIN_ENC_SW); // true=up, false=pressed
  uint32_t now = millis();

  if (swPrevUp && !up) { // pressed
    swDownMs = now;
    longFired = false;
  }
  if (!swPrevUp && up) { // released
    if (!longFired) enc.click = true;
  }
  if (!up && !longFired && (uint32_t)(now - swDownMs) > 600) {
    enc.longClick = true;
    longFired = true;
  }

  swPrevUp = up;
}

// ---------------- LCD minimal redraw ----------------
char line0[17] = {0}, line1[17] = {0};

void lcdSetLine(uint8_t row, const char* s) {
  char* dst = (row == 0) ? line0 : line1;
  for (uint8_t i = 0; i < 16; i++) {
    char c = s[i];
    if (c == 0) {
      dst[i] = ' ';
      for (uint8_t j = i + 1; j < 16; j++) dst[j] = ' ';
      break;
    }
    dst[i] = c;
  }
  dst[16] = 0;
}

void lcdFlush() {
  static char prev0[17] = {0}, prev1[17] = {0};
  if (strcmp(prev0, line0) != 0) {
    lcd.setCursor(0, 0);
    lcd.print(line0);
    strcpy(prev0, line0);
  }
  if (strcmp(prev1, line1) != 0) {
    lcd.setCursor(0, 1);
    lcd.print(line1);
    strcpy(prev1, line1);
  }
}

// format: "359.99"
void formatAngle100(char* out, uint16_t a100) {
  uint16_t deg = a100 / 100;
  uint8_t d1 = (a100 / 10) % 10;
  uint8_t d2 = a100 % 10;
  sprintf(out, "%3u.%1u%1u", deg, d1, d2);
}

// ---------------- Menu / UI ----------------
enum Screen : uint8_t {
  SCR_MAIN = 0,
  SCR_MENU,
  SCR_VIEW,
  SCR_ZERO,
  SCR_SETVALUE,
  SCR_CALMIN,
  SCR_CALMAX,
  SCR_INVERT,
};

Screen scr = SCR_MAIN;

const char* menuItems[] = { "View", "Set Zero", "Set Value", "Cal Min", "Cal Max", "Invert" };
const uint8_t MENU_N = sizeof(menuItems) / sizeof(menuItems[0]);
uint8_t menuIdx = 0;

// Set Value editor state
uint16_t target100 = 0; // 0..35999
uint16_t step100   = 1; // 1=0.01°, 10=0.1°, 100=1°, 1000=10°

static inline int16_t clampi16(int16_t v, int16_t lo, int16_t hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

void handleUI(uint16_t adc, uint16_t raw100, uint16_t shown100) {
  // consume encoder events
  int16_t d = enc.delta; enc.delta = 0;
  bool click = enc.click; enc.click = false;
  bool longClick = enc.longClick; enc.longClick = false;

  // -------- state machine --------
  if (scr == SCR_MAIN) {
    if (click) scr = SCR_MENU;
    if (longClick) doSetZero(raw100); // quick set zero from main
  }
  else if (scr == SCR_MENU) {
    if (d) {
      int16_t ni = (int16_t)menuIdx + (d > 0 ? 1 : -1);
      ni = clampi16(ni, 0, (int16_t)MENU_N - 1);
      menuIdx = (uint8_t)ni;
    }
    if (click) {
      switch (menuIdx) {
        case 0: scr = SCR_VIEW; break;
        case 1: scr = SCR_ZERO; break;
        case 2:
          scr = SCR_SETVALUE;
          target100 = shown100; // start editing from current shown value
          step100 = 1;          // 0.01°
          break;
        case 3: scr = SCR_CALMIN; break;
        case 4: scr = SCR_CALMAX; break;
        case 5: scr = SCR_INVERT; break;
      }
    }
    if (longClick) scr = SCR_MAIN;
  }
  else if (scr == SCR_SETVALUE) {
    // Rotate => change target
    if (d) {
      int32_t t = (int32_t)target100 + (d > 0 ? (int32_t)step100 : -(int32_t)step100);
      while (t < 0) t += 36000;
      while (t >= 36000) t -= 36000;
      target100 = (uint16_t)t;
    }
    // Click => change step
    if (click) {
      if (step100 == 1) step100 = 10;
      else if (step100 == 10) step100 = 100;
      else if (step100 == 100) step100 = 1000;
      else step100 = 1;
    }
    // Long => apply and back to menu
    if (longClick) {
      doSetValue(raw100, target100);
      scr = SCR_MENU;
    }
  }
  else {
    // action screens: click=do, long=back
    if (click) {
      if (scr == SCR_ZERO)   doSetZero(raw100);
      if (scr == SCR_CALMIN) doCalMin(adc);
      if (scr == SCR_CALMAX) doCalMax(adc);
      if (scr == SCR_INVERT) doInvertToggle();
      scr = SCR_MENU;
    }
    if (longClick) scr = SCR_MENU;
  }

  // -------- render --------
  char buf0[17] = {0}, buf1[17] = {0};

  if (scr == SCR_MAIN) {
    char a[8]; formatAngle100(a, shown100);
    snprintf(buf0, 17, "ABS:%s deg", a);
    snprintf(buf1, 17, "Click:MENU L:0");
  }
  else if (scr == SCR_MENU) {
    snprintf(buf0, 17, ">%s", menuItems[menuIdx]);
    snprintf(buf1, 17, "Click:OK L:Back");
  }
  else if (scr == SCR_VIEW) {
    char a[8]; formatAngle100(a, shown100);
    snprintf(buf0, 17, "Angle:%s deg", a);
    snprintf(buf1, 17, "ADC:%4u", adc);
  }
  else if (scr == SCR_ZERO) {
    snprintf(buf0, 17, "Set ZERO?");
    snprintf(buf1, 17, "Click:YES L:Back");
  }
  else if (scr == SCR_SETVALUE) {
    char t[8]; formatAngle100(t, target100);
    snprintf(buf0, 17, "Set:%s deg", t);

    if (step100 == 1)        snprintf(buf1, 17, "Step:0.01 L:OK");
    else if (step100 == 10)  snprintf(buf1, 17, "Step:0.1  L:OK");
    else if (step100 == 100) snprintf(buf1, 17, "Step:1    L:OK");
    else                     snprintf(buf1, 17, "Step:10   L:OK");
  }
  else if (scr == SCR_CALMIN) {
    snprintf(buf0, 17, "Cal MIN=%4u", adc);
    snprintf(buf1, 17, "Click:SAVE L:Back");
  }
  else if (scr == SCR_CALMAX) {
    snprintf(buf0, 17, "Cal MAX=%4u", adc);
    snprintf(buf1, 17, "Click:SAVE L:Back");
  }
  else if (scr == SCR_INVERT) {
    snprintf(buf0, 17, "Invert: %s", (S.flags & 1) ? "ON " : "OFF");
    snprintf(buf1, 17, "Click:TOG L:Back");
  }

  lcdSetLine(0, buf0);
  lcdSetLine(1, buf1);
  lcdFlush();
}

// ---------------- Timing ----------------
uint32_t lastEncTick = 0;
uint32_t lastUiTick  = 0;

void setup() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_ENC_SW, INPUT_PULLUP);

  analogReference(DEFAULT); // AVcc

  loadSettings();

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0); lcd.print("P3022 + Menu");
  lcd.setCursor(0, 1); lcd.print("Init...");
  delay(400);

  // init prevAB to current state to avoid first jump
  prevAB = ((digitalRead(PIN_ENC_A) & 1) << 1) | (digitalRead(PIN_ENC_B) & 1);
  swPrevUp = digitalRead(PIN_ENC_SW);
}

void loop() {
  uint32_t now = millis();

  // 1ms tick for encoder
  if ((uint32_t)(now - lastEncTick) >= 1) {
    lastEncTick = now;
    encoderTick1ms();
  }

  // 20ms tick for ADC + UI + LCD
  if ((uint32_t)(now - lastUiTick) >= 20) {
    lastUiTick = now;

    uint16_t adc    = readAdcAvg16();
    uint16_t raw100 = adcToAngle100(adc);       // sensor angle (calibrated, invert applied), no zero
    uint16_t shown  = applyZero100(raw100);     // displayed angle with zero offset

    handleUI(adc, raw100, shown);
  }
}
