#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

// Format angle from centidegrees (0..35999) to string "359.99"
// Example: 35999 -> "359.99", 1234 -> " 12.34"
void formatAngle100(char* out, uint16_t a100);

#endif // UTILS_H
