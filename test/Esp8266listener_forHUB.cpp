#include <Arduino.h>

void setup()
{
    // Initialize the main hardware serial (UART0 on GPIO 3/RX0 and GPIO 1/TX0)
    Serial.begin(115200);

    Serial.println("========================================");
    Serial.println("   ESP32 SNIFFER RECEIVER (UART0/USB)");
    Serial.println("========================================");
}

void loop()
{
    while (Serial.available() > 0) {
        String line = Serial.readStringUntil('\n');
        line.trim();

        if (line.length() > 0) {
            // Print the incoming ESP8266 data
            Serial.println(line);
        }
    }
}