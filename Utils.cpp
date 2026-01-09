#include "Utils.h"

void formatAngle100(char* out, uint16_t a100) {
  uint16_t deg = a100 / 100;        // Whole degrees (0..359)
  uint8_t d1 = (a100 / 10) % 10;    // First decimal (tenths)
  uint8_t d2 = a100 % 10;           // Second decimal (hundredths)
  sprintf(out, "%3u.%1u%1u", deg, d1, d2);
}
