#include <Arduino.h>
#include <SPI.h>
#include <SD.h>

#define SD_CS   5
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23

SPIClass sdSPI(VSPI);

void setup() {

    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==========================");
    Serial.println("ESP32 SD CARD TEST");
    Serial.println("==========================");

    // Start SPI
    sdSPI.begin(
        SD_SCK,
        SD_MISO,
        SD_MOSI,
        SD_CS
    );

    Serial.println("SPI started.");

    // Initialize SD card
    if (!SD.begin(SD_CS, sdSPI)) {

        Serial.println("ERROR: SD card initialization failed!");

        return;
    }

    Serial.println("SD card initialized successfully.");

    // Check card type
    uint8_t cardType = SD.cardType();

    if (cardType == CARD_NONE) {

        Serial.println("ERROR: No SD card detected.");

        return;
    }

    Serial.print("Card type: ");

    if (cardType == CARD_MMC) {
        Serial.println("MMC");
    }
    else if (cardType == CARD_SD) {
        Serial.println("SDSC");
    }
    else if (cardType == CARD_SDHC) {
        Serial.println("SDHC");
    }
    else {
        Serial.println("UNKNOWN");
    }

    // Card size
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);

    Serial.print("Card size: ");
    Serial.print(cardSize);
    Serial.println(" MB");


    // ==========================
    // WRITE TEST
    // ==========================

    Serial.println();
    Serial.println("Writing test file...");

    File file = SD.open("/test.txt", FILE_WRITE);

    if (!file) {

        Serial.println("ERROR: Could not open test.txt");

        return;
    }

    file.println("ESP32 SD CARD TEST");
    file.println("SD card is working!");
    file.println("Hello from the ESP32.");

    file.close();

    Serial.println("File written successfully.");


    // ==========================
    // READ TEST
    // ==========================

    Serial.println();
    Serial.println("Reading test file...");
    Serial.println("--------------------------");

    file = SD.open("/test.txt");

    if (!file) {

        Serial.println("ERROR: Could not open test.txt");

        return;
    }

    while (file.available()) {

        Serial.write(file.read());
    }

    file.close();

    Serial.println();
    Serial.println("--------------------------");
    Serial.println("SD CARD TEST COMPLETE");
}


void loop() {
}