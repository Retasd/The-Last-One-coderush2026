#include <Arduino.h>

extern "C" {
    #include "user_interface.h"
}

#define FIRST_CHANNEL 1
#define LAST_CHANNEL 13
#define CHANNEL_TIME 250
#define REPORT_TIME 1000
#define MAX_DEVICES 100

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

volatile uint32_t packetCount = 0;

uint8_t macList[MAX_DEVICES][6];
uint16_t deviceCount = 0;

uint8_t currentChannel = FIRST_CHANNEL;

unsigned long lastChannelChange = 0;
unsigned long lastReport = 0;

bool sameMAC(uint8_t *a, uint8_t *b)
{
    for (int i = 0; i < 6; i++) {
        if (a[i] != b[i])
            return false;
    }

    return true;
}

void ICACHE_RAM_ATTR wifiCallback(uint8_t *buf, uint16_t len)
{
    if (!buf)
        return;

    if (len < sizeof(RxControl) + 24)
        return;

    uint8_t *frame = buf + sizeof(RxControl);

    // Source MAC address
    uint8_t *mac = &frame[10];

    packetCount++;

    if (deviceCount >= MAX_DEVICES)
        return;

    for (uint16_t i = 0; i < deviceCount; i++) {
        if (sameMAC(macList[i], mac))
            return;
    }

    for (int i = 0; i < 6; i++)
        macList[deviceCount][i] = mac[i];

    deviceCount++;
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    wifi_set_opmode(STATION_MODE);

    wifi_promiscuous_enable(0);

    wifi_set_promiscuous_rx_cb(wifiCallback);

    wifi_set_channel(currentChannel);

    wifi_promiscuous_enable(1);

    lastChannelChange = millis();
    lastReport = millis();

    Serial.println("READY");
}

void loop()
{
    unsigned long now = millis();

    // Change Wi-Fi channel
    if (now - lastChannelChange >= CHANNEL_TIME) {

        lastChannelChange = now;

        currentChannel++;

        if (currentChannel > LAST_CHANNEL)
            currentChannel = FIRST_CHANNEL;

        wifi_set_channel(currentChannel);
    }

    // Send summary
    if (now - lastReport >= REPORT_TIME) {

        lastReport = now;

        Serial.print("WIFI|");
        Serial.print(currentChannel);
        Serial.print("|");
        Serial.print(packetCount);
        Serial.print("|");
        Serial.println(deviceCount);

        packetCount = 0;
    }

    yield();
}