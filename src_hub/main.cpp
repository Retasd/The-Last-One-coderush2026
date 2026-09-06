#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

// ========================================
// SETTINGS
// ========================================

#define MAX_BLE_DEVICES 100
#define REPORT_INTERVAL 1000

// ========================================
// BLE
// ========================================

BLEScan *bleScan;

uint8_t bleMacs[MAX_BLE_DEVICES][6];
uint16_t bleUnique = 0;

// ========================================
// WIFI
// ========================================

uint16_t wifiUnique = 0;
uint8_t wifiChannel = 0;
uint32_t wifiPackets = 0;

unsigned long lastReport = 0;

// ========================================
// CHECK BLE MAC
// ========================================

bool sameMac(uint8_t *a, uint8_t *b)
{
    for (int i = 0; i < 6; i++) {
        if (a[i] != b[i])
            return false;
    }

    return true;
}

// ========================================
// BLE CALLBACK
// ========================================

class BLECallbacks : public BLEAdvertisedDeviceCallbacks {

    void onResult(BLEAdvertisedDevice device)
    {
        String address =
            device.getAddress().toString().c_str();

        // Convert BLE address into bytes
        uint8_t mac[6];

        int index = 0;

        for (int i = 0; i < 17; i += 3) {

            String part =
                address.substring(i, i + 2);

            mac[index++] =
                strtoul(part.c_str(), NULL, 16);
        }

        // Check if already seen
        for (uint16_t i = 0; i < bleUnique; i++) {

            if (sameMac(bleMacs[i], mac))
                return;
        }

        // Add new device
        if (bleUnique < MAX_BLE_DEVICES) {

            for (int i = 0; i < 6; i++)
                bleMacs[bleUnique][i] = mac[i];

            bleUnique++;
        }
    }
};

// ========================================
// RESET BLE LIST
// ========================================

void resetBLE()
{
    bleUnique = 0;
}

// ========================================
// READ ESP8266
// ========================================

void readWiFi()
{
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
                        line.substring(p1 + 1, p2).toInt();

                    wifiPackets =
                        line.substring(p2 + 1, p3).toInt();

                    wifiUnique =
                        line.substring(p3 + 1).toInt();
                }
            }

            else if (line == "READY") {

                // ESP8266 connected
                Serial.println("[WIFI] READY");
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
// SETUP
// ========================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("==============================");
    Serial.println("       ESP32 SNIFFER HUB");
    Serial.println("==============================");

    // BLE
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
}

// ========================================
// LOOP
// ========================================

void loop()
{
    // Keep reading ESP8266
    readWiFi();

    // Reset BLE counter
    resetBLE();

    // Scan for 1 second
    bleScan->start(1, false);

    // Read Wi-Fi data collected during scan
    readWiFi();

    bleScan->clearResults();

    // ====================================
    // REPORT
    // ====================================

    if (millis() - lastReport >= REPORT_INTERVAL) {

        lastReport = millis();

        uint16_t total =
            wifiUnique + bleUnique;

        Serial.print("[WIFI] UNIQUE=");
        Serial.println(wifiUnique);

        Serial.print("[BLE]  UNIQUE=");
        Serial.println(bleUnique);

        Serial.print("[TOTAL] UNIQUE=");
        Serial.println(total);

        Serial.println("------------------------------");
    }

    delay(5);
}