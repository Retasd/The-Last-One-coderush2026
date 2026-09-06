#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

#define OLED_SDA 13
#define OLED_SCL 14

#define OLED_ADDRESS 0x3C
#define OLED_RESET   -1

Adafruit_SSD1306 display(
    OLED_WIDTH,
    OLED_HEIGHT,
    &Wire,
    OLED_RESET
);

void setup() {
    Serial.begin(115200);
    delay(500);

    // SDA, SCL
    Wire.begin(OLED_SDA, OLED_SCL);

    Serial.println("Starting GM009605V4.3...");

    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDRESS)) {
        Serial.println("SSD1306 initialization FAILED!");
        while (true) {
            delay(1000);
        }
    }

    Serial.println("SSD1306 initialized!");

    // Clear everything
    display.clearDisplay();
    display.display();

    delay(500);

    // --------------------------------
    // TEST 1: Full screen
    // --------------------------------
    display.fillScreen(SSD1306_WHITE);
    display.display();

    delay(1500);

    // --------------------------------
    // TEST 2: Completely blank
    // --------------------------------
    display.clearDisplay();
    display.display();

    delay(1000);

    // --------------------------------
    // TEST 3: Border
    // --------------------------------
    display.drawRect(
        0, 0,
        OLED_WIDTH,
        OLED_HEIGHT,
        SSD1306_WHITE
    );

    display.display();

    delay(1500);

    // --------------------------------
    // TEST 4: Text
    // --------------------------------
    display.clearDisplay();

    display.setTextColor(SSD1306_WHITE);

    display.setTextSize(2);
    display.setCursor(10, 5);
    display.println("ESP32");

    display.setTextSize(1);
    display.setCursor(10, 30);
    display.println("GM009605V4.3");

    display.setCursor(10, 45);
    display.println("SSD1306 128x64");

    display.setCursor(10, 56);
    display.println("WORKING!");

    display.display();

    Serial.println("Display test complete.");
}

void loop() {
}