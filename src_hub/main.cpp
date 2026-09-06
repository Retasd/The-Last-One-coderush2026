#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ========================================
// OLED
// ========================================

#define OLED_SDA 21
#define OLED_SCL 22

U8G2_SH1106_128X64_NONAME_F_HW_I2C oled(
    U8G2_R0,
    U8X8_PIN_NONE
);

// ========================================
// SETTINGS
// ========================================

#define MAX_BLE_DEVICES 100
#define REPORT_INTERVAL 1000

// ========================================
// BLE
// ========================================

BLEScan *bleScan;

String bleDevices[MAX_BLE_DEVICES];
uint16_t bleUnique = 0;

// ========================================
// WIFI
// ========================================

uint16_t wifiUnique = 0;
uint32_t wifiPackets = 0;
uint8_t wifiChannel = 0;

// ========================================
// TIMING
// ========================================

unsigned long lastReport = 0;

// ========================================
// BLE CALLBACK
// ========================================

class BLECallbacks : public BLEAdvertisedDeviceCallbacks {

    void onResult(BLEAdvertisedDevice device) {

        String address =
            device.getAddress().toString().c_str();

        // Check if already seen
        for (uint16_t i = 0; i < bleUnique; i++) {

            if (bleDevices[i] == address)
                return;
        }

        // Add new device
        if (bleUnique < MAX_BLE_DEVICES) {

            bleDevices[bleUnique] = address;
            bleUnique++;
        }
    }
};

// ========================================
// RESET BLE
// ========================================

void resetBLE() {

    bleUnique = 0;

    for (int i = 0; i < MAX_BLE_DEVICES; i++)
        bleDevices[i] = "";
}

// ========================================
// READ ESP8266
// ========================================

void readWiFi() {

    static String line;

    while (Serial.available()) {

        char c = Serial.read();

        if (c == '\n') {

            line.trim();

            if (line.startsWith("WIFI|")) {

                int p1 = line.indexOf('|');
                int p2 = line.indexOf('|', p1 + 1);
                int p3 = line.indexOf('|', p2 + 1);

                if (p1 > 0 && p2 > p1 && p3 > p2) {

                    wifiChannel =
                        line.substring(
                            p1 + 1,
                            p2
                        ).toInt();

                    wifiPackets =
                        line.substring(
                            p2 + 1,
                            p3
                        ).toInt();

                    wifiUnique =
                        line.substring(
                            p3 + 1
                        ).toInt();
                }
            }

            line = "";
        }

        else if (c != '\r') {

            if (line.length() < 100)
                line += c;
            else
                line = "";
        }
    }
}

// ========================================
// OLED DISPLAY
// ========================================

void updateOLED() {

    uint16_t total =
        wifiUnique + bleUnique;

    oled.clearBuffer();

    // Title
    oled.setFont(u8g2_font_7x13B_tr);
    oled.drawStr(0, 12, "SNIFFER HUB");

    // Divider
    oled.drawHLine(0, 15, 128);

    // WiFi
    oled.setFont(u8g2_font_7x13_tr);

    oled.drawStr(0, 30, "WIFI");

    oled.setFont(u8g2_font_7x13B_tr);

    oled.setCursor(75, 30);
    oled.print(wifiUnique);

    // BLE
    oled.setFont(u8g2_font_7x13_tr);

    oled.drawStr(0, 44, "BLE");

    oled.setFont(u8g2_font_7x13B_tr);

    oled.setCursor(75, 44);
    oled.print(bleUnique);

    // Total
    oled.setFont(u8g2_font_7x13_tr);

    oled.drawStr(0, 59, "TOTAL");

    oled.setFont(u8g2_font_7x13B_tr);

    oled.setCursor(75, 59);
    oled.print(total);

    oled.sendBuffer();
}

// ========================================
// SETUP
// ========================================

void setup() {

    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println("       ESP32 SNIFFER HUB");
    Serial.println("==============================");

    // ====================================
    // OLED
    // ====================================

    Wire.begin(OLED_SDA, OLED_SCL);

    oled.begin();

    oled.clearBuffer();

    oled.setFont(u8g2_font_7x13B_tr);

    oled.drawStr(0, 20, "SNIFFER HUB");
    oled.drawStr(0, 40, "INITIALIZING...");

    oled.sendBuffer();

    Serial.println("[OLED] READY");

    // ====================================
    // BLE
    // ====================================

    BLEDevice::init("ESP32_HUB");

    bleScan = BLEDevice::getScan();

    bleScan->setAdvertisedDeviceCallbacks(
        new BLECallbacks()
    );

    bleScan->setActiveScan(true);

    bleScan->setInterval(100);
    bleScan->setWindow(80);

    Serial.println("[BLE] READY");
    Serial.println("[WIFI] READY");

    delay(1000);
}

// ========================================
// LOOP
// ========================================

void loop() {

    // Read WiFi data
    readWiFi();

    // Start fresh BLE window
    resetBLE();

    // Scan BLE for 1 second
    bleScan->start(1, false);

    // Read any WiFi data received
    readWiFi();

    bleScan->clearResults();

    // ====================================
    // UPDATE
    // ====================================

    if (millis() - lastReport >= REPORT_INTERVAL) {

        lastReport = millis();

        uint16_t total =
            wifiUnique + bleUnique;

        // Serial output
        Serial.println();

        Serial.print("[WIFI] UNIQUE=");
        Serial.println(wifiUnique);

        Serial.print("[BLE]  UNIQUE=");
        Serial.println(bleUnique);

        Serial.print("[TOTAL] UNIQUE=");
        Serial.println(total);

        Serial.println("------------------------------");

        // OLED
        updateOLED();
    }

    delay(5);
}