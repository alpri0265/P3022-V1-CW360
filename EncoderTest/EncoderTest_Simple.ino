// Простий тестовий скетч для перевірки нашого енкодера
// Використовує нашу власну реалізацію Encoder класу
// Не потребує додаткових бібліотек

#include "Encoder.h"

Encoder encoder(2, 3, 4);  // A, B, SW pins

void setup() {
    Serial.begin(115200);
    
    encoder.begin();
    
    Serial.println("========================================");
    Serial.println("=== Simple Encoder Test ===");
    Serial.println("Rotate encoder and watch delta values");
    Serial.println("========================================");
    Serial.println();
}

void loop() {
    encoder.update();
    
    Encoder::State state = encoder.getState(true);  // Get and reset
    
    if (state.delta != 0) {
        Serial.print("Delta: ");
        Serial.print(state.delta);
        Serial.print(" (abs: ");
        Serial.print(abs(state.delta));
        Serial.print(")");
        
        // Calculate detents for different values
        Serial.print(" | Detents (div 1): ");
        Serial.print(state.delta / 1);
        Serial.print(", (div 2): ");
        Serial.print(state.delta / 2);
        Serial.print(", (div 4): ");
        Serial.print(state.delta / 4);
        Serial.print(", (div 8): ");
        Serial.print(state.delta / 8);
        Serial.print(", (div 16): ");
        Serial.println(state.delta / 16);
    }
    
    if (state.click) {
        Serial.println("CLICK!");
    }
    
    if (state.longClick) {
        Serial.println("LONG CLICK!");
    }
    
    delay(1);  // Small delay
}
