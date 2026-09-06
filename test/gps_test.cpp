#include <Arduino.h>

#define GPS_RX 16
#define GPS_TX 17
#define GPS_BAUD 9600

HardwareSerial GPS(2);

void setup() {
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("====================");
    Serial.println("NEO-6M GPS TEST");
    Serial.println("====================");

    GPS.begin(
        GPS_BAUD,
        SERIAL_8N1,
        GPS_RX,
        GPS_TX
    );

    Serial.println("GPS UART started.");
    Serial.println("Waiting for GPS data...");
    Serial.println();
}

void loop() {

    while (GPS.available()) {

        char c = GPS.read();

        Serial.write(c);
    }
}