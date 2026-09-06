#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

BLEScan* pBLEScan;

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("BLE Found: ");
        Serial.println(advertisedDevice.getAddress().toString().c_str());
    }
};

void setup() {
    Serial.begin(115200);
    delay(2000); // Give serial time to settle
    
    Serial.println("\n[BOOT] Starting ESP32 Isolation Test...");

    // Initialize BLE step-by-step
    Serial.println("[INIT] Initializing BLE Device...");
    BLEDevice::init("ESP32_Scanner");
    
    Serial.println("[INIT] Getting BLE Scan instance...");
    pBLEScan = BLEDevice::getScan();
    
    Serial.println("[INIT] Setting callbacks...");
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    
    Serial.println("[INIT] Configuring active scan...");
    pBLEScan->setActiveScan(true);
    pBLEScan->setInterval(100);
    pBLEScan->setWindow(99);
    
    Serial.println("[SETUP COMPLETE] Entering loop...");
}

void loop() {
    Serial.println("[SCAN] Starting BLE scan for 3 seconds...");
    
    // Perform block scan
    BLEScanResults foundDevices = pBLEScan->start(3, false);
    
    Serial.print("[SCAN DONE] Devices found: ");
    Serial.println(foundDevices.getCount());
    
    pBLEScan->clearResults();
    delay(2000);
}