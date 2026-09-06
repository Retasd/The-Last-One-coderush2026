#include <Arduino.h>

extern "C" {
    #include "user_interface.h"
}

// ========================================
// SETTINGS
// ========================================

#define FIRST_CHANNEL       1
#define LAST_CHANNEL        13

#define CHANNEL_TIME_MS     250
#define REPORT_INTERVAL_MS  5000

#define MAX_DEVICES         300

// Approximate RSSI at 1 meter
#define RSSI_AT_1M          -40.0

// Typical indoor environment
#define PATH_LOSS_EXPONENT  2.7


// ========================================
// ESP8266 RX METADATA
// ========================================

struct RxControl {
    signed rssi : 8;
    unsigned rate : 4;
    unsigned is_group : 1;
    unsigned : 1;
    unsigned sig_mode : 2;

    unsigned legacy_length : 12;
    unsigned damatch0 : 1;
    unsigned damatch1 : 1;
    unsigned bssidmatch0 : 1;
    unsigned bssidmatch1 : 1;

    unsigned MCS : 7;
    unsigned CWB : 1;
    unsigned HT_length : 16;

    unsigned Smoothing : 1;
    unsigned Not_Sounding : 1;
    unsigned : 1;
    unsigned Aggregation : 1;
    unsigned STBC : 2;
    unsigned FEC_CODING : 1;
    unsigned SGI : 1;

    unsigned rxend_state : 8;
    unsigned ampdu_cnt : 8;
    unsigned channel : 4;
    unsigned : 12;
};


// ========================================
// DEVICE STRUCTURE
// ========================================

struct Device {

    uint8_t mac[6];

    uint32_t packets;

    int8_t strongestRSSI;

    int8_t lastRSSI;

    uint8_t lastChannel;

    unsigned long lastSeen;
};


// ========================================
// GLOBAL DATA
// ========================================

Device devices[MAX_DEVICES];

volatile uint16_t deviceCount = 0;
volatile uint32_t totalPackets = 0;

volatile uint8_t currentChannel = 1;

unsigned long lastChannelChange = 0;
unsigned long lastReport = 0;


// ========================================
// MAC COMPARISON
// ========================================

bool sameMAC(uint8_t *a, uint8_t *b)
{
    for (int i = 0; i < 6; i++) {

        if (a[i] != b[i]) {
            return false;
        }
    }

    return true;
}


// ========================================
// PRINT MAC
// ========================================

void printMAC(uint8_t *mac)
{
    for (int i = 0; i < 6; i++) {

        if (mac[i] < 0x10) {
            Serial.print("0");
        }

        Serial.print(mac[i], HEX);

        if (i < 5) {
            Serial.print(":");
        }
    }
}


// ========================================
// RSSI → DISTANCE
// ========================================

float estimateDistance(int rssi)
{
    float exponent =
        (RSSI_AT_1M - rssi) /
        (10.0 * PATH_LOSS_EXPONENT);

    return pow(10.0, exponent);
}


// ========================================
// RSSI → PROXIMITY
// ========================================

const char* getProximity(int rssi)
{
    if (rssi >= -45) {
        return "VERY CLOSE";
    }

    if (rssi >= -55) {
        return "CLOSE";
    }

    if (rssi >= -65) {
        return "MEDIUM";
    }

    if (rssi >= -75) {
        return "FAR";
    }

    return "VERY FAR";
}


// ========================================
// RECORD DEVICE
// ========================================

void recordDevice(
    uint8_t *mac,
    int8_t rssi,
    uint8_t channel
)
{
    totalPackets++;

    // Existing device?
    for (uint16_t i = 0; i < deviceCount; i++) {

        if (sameMAC(devices[i].mac, mac)) {

            devices[i].packets++;

            devices[i].lastRSSI = rssi;

            devices[i].lastChannel = channel;

            devices[i].lastSeen = millis();

            if (rssi > devices[i].strongestRSSI) {
                devices[i].strongestRSSI = rssi;
            }

            return;
        }
    }


    // New device
    if (deviceCount < MAX_DEVICES) {

        for (int i = 0; i < 6; i++) {
            devices[deviceCount].mac[i] = mac[i];
        }

        devices[deviceCount].packets = 1;

        devices[deviceCount].strongestRSSI = rssi;

        devices[deviceCount].lastRSSI = rssi;

        devices[deviceCount].lastChannel = channel;

        devices[deviceCount].lastSeen = millis();

        deviceCount++;
    }
}


// ========================================
// PACKET CALLBACK
// ========================================

void wifiSnifferCallback(uint8_t *buf, uint16_t len)
{
    if (buf == nullptr) {
        return;
    }

    // RX metadata + minimum 802.11 header
    if (len < sizeof(RxControl) + 24) {
        return;
    }


    // RX metadata
    RxControl *rx =
        (RxControl *)buf;


    // Actual 802.11 frame
    uint8_t *frame =
        buf + sizeof(RxControl);


    // Source MAC
    uint8_t *sourceMAC =
        &frame[10];


    int8_t rssi =
        rx->rssi;


    uint8_t channel =
        rx->channel;


    // Some SDK versions report zero here.
    // Use our known current channel.
    if (channel < 1 || channel > 13) {
        channel = currentChannel;
    }


    recordDevice(
        sourceMAC,
        rssi,
        channel
    );
}


// ========================================
// PRINT DEVICE
// ========================================

void printDevice(uint16_t index)
{
    Device &d = devices[index];

    Serial.print("#");
    Serial.print(index + 1);

    Serial.print("  ");

    printMAC(d.mac);

    Serial.print(" | RSSI: ");
    Serial.print(d.strongestRSSI);
    Serial.print(" dBm");

    Serial.print(" | CH: ");
    Serial.print(d.lastChannel);

    Serial.print(" | Packets: ");
    Serial.print(d.packets);

    Serial.print(" | ~");

    float distance =
        estimateDistance(d.strongestRSSI);

    if (distance < 1.0) {
        Serial.print(distance, 2);
    }
    else if (distance < 10.0) {
        Serial.print(distance, 1);
    }
    else {
        Serial.print(distance, 0);
    }

    Serial.print(" m");

    Serial.print(" | ");

    Serial.println(
        getProximity(d.strongestRSSI)
    );
}


// ========================================
// REPORT
// ========================================

void printReport()
{
    Serial.println();
    Serial.println();
    Serial.println("========================================");
    Serial.println("          WIFI SNIFFER REPORT");
    Serial.println("========================================");

    Serial.print("Current channel: ");
    Serial.println(currentChannel);

    Serial.print("Total packets:   ");
    Serial.println(totalPackets);

    Serial.print("Unique devices:  ");
    Serial.println(deviceCount);

    Serial.println();

    Serial.println("DEVICE LIST");
    Serial.println("----------------------------------------");


    for (uint16_t i = 0; i < deviceCount; i++) {
        printDevice(i);
    }


    // ====================================
    // PROXIMITY COUNTS
    // ====================================

    uint16_t veryClose = 0;
    uint16_t close = 0;
    uint16_t medium = 0;
    uint16_t far = 0;
    uint16_t veryFar = 0;


    for (uint16_t i = 0; i < deviceCount; i++) {

        int rssi =
            devices[i].strongestRSSI;


        if (rssi >= -45) {
            veryClose++;
        }
        else if (rssi >= -55) {
            close++;
        }
        else if (rssi >= -65) {
            medium++;
        }
        else if (rssi >= -75) {
            far++;
        }
        else {
            veryFar++;
        }
    }


    Serial.println();
    Serial.println("PROXIMITY SUMMARY");
    Serial.println("----------------------------------------");

    Serial.print("Very close : ");
    Serial.println(veryClose);

    Serial.print("Close      : ");
    Serial.println(close);

    Serial.print("Medium     : ");
    Serial.println(medium);

    Serial.print("Far        : ");
    Serial.println(far);

    Serial.print("Very far   : ");
    Serial.println(veryFar);

    Serial.println("========================================");
}


// ========================================
// SETUP
// ========================================

void setup()
{
    Serial.begin(115200);

    delay(1000);

    Serial.println();
    Serial.println("========================================");
    Serial.println("        ESP8266 WIFI SNIFFER");
    Serial.println("========================================");

    Serial.println("Channels: 1-13");
    Serial.println("Channel dwell: 250 ms");
    Serial.println("RSSI: ENABLED");
    Serial.println("Unique MAC tracking: ENABLED");
    Serial.println();


    // Station mode
    wifi_set_opmode(STATION_MODE);


    // Disable promiscuous mode
    wifi_promiscuous_enable(0);


    // Register callback
    wifi_set_promiscuous_rx_cb(
        wifiSnifferCallback
    );


    // Start channel 1
    currentChannel =
        FIRST_CHANNEL;

    wifi_set_channel(
        currentChannel
    );


    // Enable promiscuous mode
    wifi_promiscuous_enable(1);


    lastChannelChange =
        millis();

    lastReport =
        millis();


    Serial.println("Promiscuous mode ENABLED");
    Serial.println("Scanning...");
    Serial.println();
}


// ========================================
// LOOP
// ========================================

void loop()
{
    unsigned long now =
        millis();


    // ====================================
    // CHANNEL HOPPING
    // ====================================

    if (
        now - lastChannelChange
        >= CHANNEL_TIME_MS
    ) {

        lastChannelChange =
            now;


        currentChannel++;


        if (
            currentChannel >
            LAST_CHANNEL
        ) {

            currentChannel =
                FIRST_CHANNEL;
        }


        wifi_set_channel(
            currentChannel
        );
    }


    // ====================================
    // REPORT
    // ====================================

    if (
        now - lastReport
        >= REPORT_INTERVAL_MS
    ) {

        lastReport =
            now;

        printReport();
    }


    delay(1);
}