#include <EncButton.h>

#include <EncButton.h>

// Тестовий скетч для перевірки роботи енкодера
// Використовує бібліотеку EncButton для аналізу поведінки енкодера
// 
// ІНСТРУКЦІЯ:
// 1. Відкрийте цю папку (EncoderTest) як окремий проект в Arduino IDE
// 2. Завантажте скетч на Arduino
// 3. Відкрийте Serial Monitor (115200 baud)
// 4. Обертайте енкодер і дивіться на значення counter
// 5. Повідомте, скільки кроків (counter) на один фізичний клік енкодера

// #define EB_NO_FOR           // відключити підтримку pressFor/holdFor/stepFor і лічильник степів (економить 2 байти оперативки)
// #define EB_NO_CALLBACK      // відключити обробник подій attach (економить 2 байти оперативки)
// #define EB_NO_COUNTER       // відключити лічильник енкодера (економить 4 байти оперативки)
// #define EB_NO_BUFFER        // відключити буферизацію енкодера (економить 1 байт оперативки)

// #define EB_DEB_TIME 50      // таймаут гасіння дрижання кнопки (кнопка)
// #define EB_CLICK_TIME 500   // таймаут очікування кліків (кнопка)
// #define EB_HOLD_TIME 600    // таймаут утримання (кнопка)
// #define EB_STEP_TIME 200    // таймаут імпульсного утримання (кнопка)
// #define EB_FAST_TIME 30     // таймаут швидкого повороту (енкодер)
// #define EB_TOUT_TIME 1000   // таймаут дії (кнопка і енкодер)

#include <EncButton.h>
EncButton eb(2, 3, 4);
// EncButton eb(2, 3, 4, INPUT); // + режим пінів енкодера
// EncButton eb(2, 3, 4, INPUT, INPUT_PULLUP); // + режим пінів кнопки

int32_t lastCounter = 0;  // Для відстеження зміни counter

void setup() {
    Serial.begin(115200);

    // показані значення за замовчуванням
    eb.setBtnLevel(LOW);
    eb.setClickTimeout(500);
    eb.setDebTimeout(50);
    eb.setHoldTimeout(600);
    eb.setStepTimeout(200);
    eb.setTimeout(1000);

    eb.setEncReverse(0);
    eb.setEncType(EB_STEP4_LOW);
    eb.setFastTimeout(30);

    // скинути лічильник енкодера
    eb.counter = 0;
    lastCounter = 0;
    
    Serial.println("========================================");
    Serial.println("=== Encoder Test Started ===");
    Serial.println("Rotate encoder to see output");
    Serial.println("Watch 'counter' value - it shows steps per click");
    Serial.println("========================================");
    Serial.println();
}

void loop() {
    eb.tick();

    // Відстеження зміни counter для визначення кроків на клік
    if (eb.counter != lastCounter) {
        int32_t delta = eb.counter - lastCounter;
        Serial.print("Counter changed: ");
        Serial.print(lastCounter);
        Serial.print(" -> ");
        Serial.print(eb.counter);
        Serial.print(" (delta: ");
        Serial.print(delta);
        Serial.print(", abs: ");
        Serial.print(abs(delta));
        Serial.println(" steps)");
        lastCounter = eb.counter;
    }

    // обробка повороту загальна
    if (eb.turn()) {
        Serial.print("TURN: dir=");
        Serial.print(eb.dir());
        Serial.print(", fast=");
        Serial.print(eb.fast());
        Serial.print(", hold=");
        Serial.print(eb.pressing());
        Serial.print(", counter=");
        Serial.print(eb.counter);
        Serial.print(", clicks=");
        Serial.println(eb.getClicks());
    }

    // обробка повороту роздільна
    if (eb.left()) {
        Serial.print("LEFT - counter=");
        Serial.println(eb.counter);
    }
    if (eb.right()) {
        Serial.print("RIGHT - counter=");
        Serial.println(eb.counter);
    }
    if (eb.leftH()) Serial.println("leftH");
    if (eb.rightH()) Serial.println("rightH");

    // кнопка
    if (eb.press()) Serial.println("press");
    if (eb.click()) Serial.println("click");

    if (eb.release()) {
      Serial.println("--- RELEASE ---");
      Serial.print("clicks: ");
      Serial.print(eb.getClicks());
      Serial.print(", steps: ");
      Serial.print(eb.getSteps());
      Serial.print(", press for: ");
      Serial.print(eb.pressFor());
      Serial.print(", hold for: ");
      Serial.print(eb.holdFor());
      Serial.print(", step for: ");
      Serial.println(eb.stepFor());
      Serial.println();
    }

    // таймаут
    if (eb.timeout()) Serial.println("timeout!");

    // утримання
    if (eb.hold()) Serial.println("hold");
    if (eb.hold(3)) Serial.println("hold 3");

    // імпульсне утримання
    if (eb.step()) Serial.println("step");
    if (eb.step(3)) Serial.println("step 3");

    // відпущена після імпульсного утримання
    if (eb.releaseStep()) Serial.println("release step");
    if (eb.releaseStep(3)) Serial.println("release step 3");

    // відпущена після утримання
    if (eb.releaseHold()) Serial.println("release hold");
    if (eb.releaseHold(2)) Serial.println("release hold 2");

    // перевірка на кількість кліків
    if (eb.hasClicks(3)) Serial.println("has 3 clicks");

    // вивести кількість кліків
    if (eb.hasClicks()) {
        Serial.print("has clicks: ");
        Serial.println(eb.getClicks());
    }
}
