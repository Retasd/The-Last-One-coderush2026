#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>

#include <U8g2lib.h>

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include <TinyGPS++.h>

#include <BLEDevice.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>

#include <WiFi.h>
#include <WebServer.h>

// =====================================================
// PIN CONFIGURATION
// =====================================================

// BIG SH1106
#define BIG_SDA 21
#define BIG_SCL 22

// MINI SSD1306
#define MINI_SDA 13
#define MINI_SCL 14

// GPS
#define GPS_RX 16
#define GPS_TX 17

// SD CARD
#define SD_CS   5
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23

// MODE BUTTON
#define MODE_BUTTON 27


// =====================================================
// BIG OLED - SH1106
// =====================================================

U8G2_SH1106_128X64_NONAME_F_HW_I2C bigOLED(
    U8G2_R0,
    U8X8_PIN_NONE
);


// =====================================================
// MINI OLED - SSD1306
// =====================================================

#define MINI_WIDTH   128
#define MINI_HEIGHT  64
#define MINI_ADDRESS 0x3C
#define MINI_RESET   -1

TwoWire miniWire = TwoWire(1);

Adafruit_SSD1306 miniOLED(
    MINI_WIDTH,
    MINI_HEIGHT,
    &miniWire,
    MINI_RESET
);


// =====================================================
// SD CARD
// =====================================================

// IMPORTANT:
// This is the exact VSPI configuration from your
// working SD card test.

SPIClass sdSPI(VSPI);

bool sdOK = false;


// =====================================================
// GPS
// =====================================================

#define GPS_BAUD 9600

HardwareSerial GPS(2);

TinyGPSPlus gps;


// =====================================================
// BLE
// =====================================================

#define MAX_BLE 100

BLEScan *bleScan;

String bleList[MAX_BLE];

uint16_t bleUnique = 0;


// =====================================================
// WIFI DATA FROM ESP8266
// =====================================================

uint8_t wifiChannel = 0;

uint32_t wifiPackets = 0;

uint16_t wifiUnique = 0;

bool board1Connected = false;

unsigned long lastWiFiData = 0;


// =====================================================
// BOARD STATUS
// =====================================================

bool board2Online = true;


// =====================================================
// SPARK MODES
// =====================================================

enum SparkMode {
    COLLECTION,
    WEBSERVER
};

SparkMode currentMode = COLLECTION;


// =====================================================
// WEB SERVER
// =====================================================

WebServer server(80);

const char *AP_SSID = "SPARK_HUB";
const char *AP_PASSWORD = "spark1234";


// =====================================================
// COLLECTION / CSV
// =====================================================

File logFile;

String currentFile = "";

unsigned long lastLog = 0;

#define LOG_INTERVAL 1000


// =====================================================
// BUTTON
// =====================================================

bool lastButtonState = HIGH;

unsigned long lastButtonPress = 0;

#define BUTTON_DEBOUNCE 300


// =====================================================
// DISPLAY
// =====================================================

unsigned long lastDisplay = 0;

#define DISPLAY_INTERVAL 250


// =====================================================
// BLE CALLBACK
// =====================================================

class BLECallbacks :
    public BLEAdvertisedDeviceCallbacks {

    void onResult(BLEAdvertisedDevice device) {

        String addr =
            device.getAddress()
                .toString()
                .c_str();

        for (
            uint16_t i = 0;
            i < bleUnique;
            i++
        ) {

            if (bleList[i] == addr)
                return;
        }

        if (bleUnique < MAX_BLE) {

            bleList[bleUnique] = addr;

            bleUnique++;
        }
    }
};


// =====================================================
// RESET BLE
// =====================================================

void resetBLE() {

    bleUnique = 0;
}


// =====================================================
// READ ESP8266 WIFI DATA
// =====================================================

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

                if (
                    p1 > 0 &&
                    p2 > p1 &&
                    p3 > p2
                ) {

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

                    board1Connected = true;

                    lastWiFiData = millis();
                }
            }

            line = "";
        }

        else if (c != '\r') {

            if (line.length() < 100) {

                line += c;

            } else {

                line = "";
            }
        }
    }

    // Board 1 timeout

    if (
        millis() - lastWiFiData > 3000
    ) {

        board1Connected = false;
    }
}


// =====================================================
// GPS READING
// =====================================================

void readGPS() {

    while (GPS.available()) {

        gps.encode(
            GPS.read()
        );
    }
}


// =====================================================
// GPS TIME
// =====================================================

String getGPSTime() {

    if (
        !gps.date.isValid() ||
        !gps.time.isValid()
    ) {

        return "";
    }

    char buffer[32];

    snprintf(
        buffer,
        sizeof(buffer),
        "%04d-%02d-%02d %02d:%02d:%02d",

        gps.date.year(),
        gps.date.month(),
        gps.date.day(),

        gps.time.hour(),
        gps.time.minute(),
        gps.time.second()
    );

    return String(buffer);
}


// =====================================================
// FIND NEXT CSV FILE
// =====================================================

String getNextFileName() {

    for (
        int i = 1;
        i <= 999;
        i++
    ) {

        char name[32];

        snprintf(
            name,
            sizeof(name),
            "/COLLECTION_%03d.CSV",
            i
        );

        if (!SD.exists(name)) {

            return String(name);
        }
    }

    return "/COLLECTION_999.CSV";
}


// =====================================================
// START NEW COLLECTION
// =====================================================

void startCollection() {

    if (!sdOK) {

        Serial.println(
            "[SD] NOT AVAILABLE"
        );

        currentFile = "";

        return;
    }

    currentFile =
        getNextFileName();

    logFile =
        SD.open(
            currentFile,
            FILE_WRITE
        );

    if (!logFile) {

        Serial.println(
            "[SD] FILE ERROR"
        );

        currentFile = "";

        return;
    }

    // CSV HEADER

    logFile.println(
        "timestamp,latitude,longitude,altitude,satellites,fix,wifi_channel,wifi_packets,wifi_unique,ble_unique"
    );

    logFile.flush();

    Serial.print(
        "[SD] NEW COLLECTION: "
    );

    Serial.println(
        currentFile
    );
}


// =====================================================
// CLOSE COLLECTION
// =====================================================

void closeCollection() {

    if (logFile) {

        logFile.flush();

        logFile.close();

        Serial.println(
            "[SD] COLLECTION CLOSED"
        );
    }
}


// =====================================================
// WRITE CSV DATA
// =====================================================

void recordData() {

    if (
        currentMode != COLLECTION
    ) {

        return;
    }

    if (!logFile) {

        return;
    }


    // -----------------------------
    // GPS
    // -----------------------------

    String timestamp =
        getGPSTime();

    String lat = "";

    String lon = "";

    String alt = "";

    int satellites = 0;

    int fix = 0;


    if (
        gps.location.isValid()
    ) {

        lat =
            String(
                gps.location.lat(),
                6
            );

        lon =
            String(
                gps.location.lng(),
                6
            );

        fix = 1;
    }


    if (
        gps.altitude.isValid()
    ) {

        alt =
            String(
                gps.altitude.meters(),
                1
            );
    }


    if (
        gps.satellites.isValid()
    ) {

        satellites =
            gps.satellites.value();
    }


    // -----------------------------
    // CSV
    // -----------------------------

    logFile.print(timestamp);

    logFile.print(",");

    logFile.print(lat);

    logFile.print(",");

    logFile.print(lon);

    logFile.print(",");

    logFile.print(alt);

    logFile.print(",");

    logFile.print(satellites);

    logFile.print(",");

    logFile.print(fix);

    logFile.print(",");

    logFile.print(wifiChannel);

    logFile.print(",");

    logFile.print(wifiPackets);

    logFile.print(",");

    logFile.print(wifiUnique);

    logFile.print(",");

    logFile.println(bleUnique);

    logFile.flush();


    // Serial debug

    Serial.print("[CSV] ");

    Serial.print(timestamp);

    Serial.print(" WIFI=");

    Serial.print(wifiUnique);

    Serial.print(" BLE=");

    Serial.println(bleUnique);
}


// =====================================================
// MINI OLED
// =====================================================

void drawMiniOLED() {

    miniOLED.clearDisplay();

    miniOLED.setTextColor(
        SSD1306_WHITE
    );


    // S.P.A.R.K.

    miniOLED.setTextSize(2);

    miniOLED.setCursor(
        18,
        2
    );

    miniOLED.println(
        "S.P.A.R.K."
    );


    // Divider

    miniOLED.drawLine(
        0,
        23,
        127,
        23,
        SSD1306_WHITE
    );


    // Mode label

    miniOLED.setTextSize(1);

    miniOLED.setCursor(
        0,
        31
    );

    miniOLED.println(
        "OPERATING MODE"
    );


    // Mode

    miniOLED.setTextSize(2);

    miniOLED.setCursor(
        5,
        43
    );

    if (
        currentMode == COLLECTION
    ) {

        miniOLED.println(
            "COLLECTION"
        );

    } else {

        miniOLED.println(
            "WEBSERVER"
        );
    }


    miniOLED.display();
}


// =====================================================
// BIG OLED - COLLECTION
// =====================================================

void drawCollectionOLED() {

    bigOLED.clearBuffer();


    // Title

    bigOLED.setFont(
        u8g2_font_7x13B_tr
    );

    bigOLED.drawStr(
        0,
        11,
        "S.P.A.R.K. HUB"
    );

    bigOLED.drawHLine(
        0,
        14,
        128
    );


    // Smaller font

    bigOLED.setFont(
        u8g2_font_6x10_tr
    );


    // Board status

    bigOLED.setCursor(
        0,
        25
    );

    bigOLED.print(
        "B1:"
    );

    bigOLED.print(
        board1Connected
        ? "OK"
        : "OFF"
    );


    bigOLED.setCursor(
        65,
        25
    );

    bigOLED.print(
        "B2:"
    );

    bigOLED.print(
        board2Online
        ? "OK"
        : "ERR"
    );


    // WIFI

    bigOLED.setCursor(
        0,
        36
    );

    bigOLED.print(
        "WIFI:"
    );

    bigOLED.print(
        wifiUnique
    );


    // BLE

    bigOLED.setCursor(
        65,
        36
    );

    bigOLED.print(
        "BLE:"
    );

    bigOLED.print(
        bleUnique
    );


    // GPS status

    bigOLED.setCursor(
        0,
        47
    );

    bigOLED.print(
        "GPS:"
    );

    if (
        gps.location.isValid()
    ) {

        bigOLED.print(
            "LOCK"
        );

    } else {

        bigOLED.print(
            "SEARCH"
        );
    }


    // Satellites

    bigOLED.setCursor(
        75,
        47
    );

    bigOLED.print(
        "SAT:"
    );

    if (
        gps.satellites.isValid()
    ) {

        bigOLED.print(
            gps.satellites.value()
        );

    } else {

        bigOLED.print(
            "0"
        );
    }


    // Coordinates

    bigOLED.setCursor(
        0,
        58
    );

    if (
        gps.location.isValid()
    ) {

        bigOLED.print(
            gps.location.lat(),
            4
        );

        bigOLED.print(
            ","
        );

        bigOLED.print(
            gps.location.lng(),
            4
        );

    } else {

        bigOLED.print(
            "GPS NO FIX"
        );
    }


    bigOLED.sendBuffer();
}


// =====================================================
// BIG OLED - WEBSERVER
// =====================================================

void drawWebServerOLED() {

    bigOLED.clearBuffer();


    // Title

    bigOLED.setFont(
        u8g2_font_7x13B_tr
    );

    bigOLED.drawStr(
        0,
        11,
        "S.P.A.R.K. WEB"
    );

    bigOLED.drawHLine(
        0,
        14,
        128
    );


    bigOLED.setFont(
        u8g2_font_6x10_tr
    );


    // WiFi name

    bigOLED.setCursor(
        0,
        26
    );

    bigOLED.print(
        "WIFI: "
    );

    bigOLED.println(
        AP_SSID
    );


    // Password

    bigOLED.setCursor(
        0,
        38
    );

    bigOLED.print(
        "PASS: "
    );

    bigOLED.println(
        AP_PASSWORD
    );


    // IP

    bigOLED.setCursor(
        0,
        50
    );

    bigOLED.print(
        "IP: 192.168.4.1"
    );


    // File count

    bigOLED.setCursor(
        0,
        62
    );

    bigOLED.print(
        "FILES: "
    );


    if (sdOK) {

        File root =
            SD.open("/");

        int count = 0;


        if (root) {

            File file =
                root.openNextFile();


            while (file) {

                if (
                    !file.isDirectory()
                ) {

                    String name =
                        file.name();

                    if (
                        name.endsWith(
                            ".CSV"
                        )
                    ) {

                        count++;
                    }
                }

                file.close();

                file =
                    root.openNextFile();
            }

            root.close();
        }


        bigOLED.print(
            count
        );

    } else {

        bigOLED.print(
            "SD ERR"
        );
    }


    bigOLED.sendBuffer();
}


// =====================================================
// CREATE WEB PAGE
// =====================================================

String makeWebPage() {

    String html;


    html +=
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<meta name='viewport' "
        "content='width=device-width,initial-scale=1'>"
        "<title>S.P.A.R.K.</title>"
        "<style>"
        "body{font-family:Arial;"
        "background:#111;color:#fff;"
        "padding:20px}"
        "a{display:block;"
        "padding:15px;"
        "margin:10px 0;"
        "background:#333;"
        "color:#fff;"
        "text-decoration:none;"
        "border-radius:8px}"
        "</style>"
        "</head>"
        "<body>";


    html +=
        "<h1>S.P.A.R.K.</h1>";


    html +=
        "<p>Collection Data</p>";


    if (!sdOK) {

        html +=
            "<p>SD card error.</p>";

    } else {

        File root =
            SD.open("/");


        if (!root) {

            html +=
                "<p>SD card error.</p>";

        } else {

            File file =
                root.openNextFile();


            while (file) {

                if (
                    !file.isDirectory()
                ) {

                    String name =
                        file.name();


                    if (
                        name.endsWith(
                            ".CSV"
                        )
                    ) {

                        String link =
                            "<a href='/download?file=";


                        link += name;

                        link +=
                            "'>";


                        link += name;

                        link +=
                            " - DOWNLOAD</a>";


                        html += link;
                    }
                }


                file.close();

                file =
                    root.openNextFile();
            }


            root.close();
        }
    }


    html +=
        "</body></html>";


    return html;
}


// =====================================================
// WEB ROOT
// =====================================================

void handleRoot() {

    server.send(
        200,
        "text/html",
        makeWebPage()
    );
}


// =====================================================
// DOWNLOAD CSV
// =====================================================

void handleDownload() {

    if (
        !server.hasArg("file")
    ) {

        server.send(
            400,
            "text/plain",
            "Missing file"
        );

        return;
    }


    String fileName =
        server.arg("file");


    // Security check

    if (
        !fileName.endsWith(
            ".CSV"
        )
    ) {

        server.send(
            400,
            "text/plain",
            "Invalid file"
        );

        return;
    }


    // Only allow simple filenames

    if (
        fileName.indexOf("..") >= 0
    ) {

        server.send(
            400,
            "text/plain",
            "Invalid file"
        );

        return;
    }


    String path =
        fileName;


    if (
        !path.startsWith("/")
    ) {

        path =
            "/" + path;
    }


    File file =
        SD.open(path);


    if (!file) {

        server.send(
            404,
            "text/plain",
            "File not found"
        );

        return;
    }


    server.streamFile(
        file,
        "text/csv"
    );


    file.close();
}


// =====================================================
// START WEB SERVER
// =====================================================

void startWebServer() {

    Serial.println(
        "[WEB] Starting AP..."
    );


    WiFi.mode(
        WIFI_AP
    );


    WiFi.softAP(
        AP_SSID,
        AP_PASSWORD
    );


    Serial.print(
        "[WEB] SSID: "
    );

    Serial.println(
        AP_SSID
    );


    Serial.print(
        "[WEB] PASSWORD: "
    );

    Serial.println(
        AP_PASSWORD
    );


    Serial.print(
        "[WEB] IP: "
    );

    Serial.println(
        WiFi.softAPIP()
    );


    // Web routes

    server.on(
        "/",
        handleRoot
    );


    server.on(
        "/download",
        handleDownload
    );


    server.begin();


    Serial.println(
        "[WEB] SERVER READY"
    );
}


// =====================================================
// STOP WEB SERVER
// =====================================================

void stopWebServer() {

    server.stop();


    WiFi.softAPdisconnect(
        true
    );


    WiFi.mode(
        WIFI_OFF
    );


    Serial.println(
        "[WEB] SERVER STOPPED"
    );
}


// =====================================================
// SWITCH MODE
// =====================================================

void switchMode() {

    if (
        currentMode == COLLECTION
    ) {

        // ---------------------------------
        // COLLECTION -> WEBSERVER
        // ---------------------------------

        closeCollection();


        currentMode =
            WEBSERVER;


        startWebServer();


        Serial.println(
            "[SPARK] MODE = WEBSERVER"
        );

    } else {

        // ---------------------------------
        // WEBSERVER -> COLLECTION
        // ---------------------------------

        stopWebServer();


        currentMode =
            COLLECTION;


        // IMPORTANT:
        // Every return to Collection creates
        // a completely NEW CSV file.

        startCollection();


        Serial.println(
            "[SPARK] MODE = COLLECTION"
        );
    }


    drawMiniOLED();
}


// =====================================================
// BUTTON
// =====================================================

void checkButton() {

    bool state =
        digitalRead(
            MODE_BUTTON
        );


    if (
        state == LOW &&
        lastButtonState == HIGH
    ) {

        if (
            millis() - lastButtonPress >
            BUTTON_DEBOUNCE
        ) {

            lastButtonPress =
                millis();


            switchMode();
        }
    }


    lastButtonState =
        state;
}


// =====================================================
// SETUP
// =====================================================

void setup() {

    Serial.begin(
        115200
    );


    delay(1000);


    Serial.println();

    Serial.println(
        "=============================="
    );

    Serial.println(
        "       S.P.A.R.K. HUB"
    );

    Serial.println(
        "=============================="
    );


    // =================================================
    // BUTTON
    // =================================================

    pinMode(
        MODE_BUTTON,
        INPUT_PULLUP
    );


    // =================================================
    // BIG OLED
    // =================================================

    Wire.begin(
        BIG_SDA,
        BIG_SCL
    );


    bigOLED.begin();


    Serial.println(
        "[BIG OLED] READY"
    );


    // =================================================
    // MINI OLED
    // =================================================

    miniWire.begin(
        MINI_SDA,
        MINI_SCL
    );


    if (
        !miniOLED.begin(
            SSD1306_SWITCHCAPVCC,
            MINI_ADDRESS
        )
    ) {

        Serial.println(
            "[MINI OLED] FAILED"
        );

    } else {

        Serial.println(
            "[MINI OLED] READY"
        );
    }


    // =================================================
    // GPS
    // =================================================

    GPS.begin(
        GPS_BAUD,
        SERIAL_8N1,
        GPS_RX,
        GPS_TX
    );


    Serial.println(
        "[GPS] UART2 READY"
    );


    // =================================================
    // SD CARD
    // =================================================
    //
    // EXACT SAME CONFIGURATION AS YOUR
    // WORKING SD CARD TEST.
    //

    sdSPI.begin(
        SD_SCK,
        SD_MISO,
        SD_MOSI,
        SD_CS
    );


    Serial.println(
        "[SD] SPI STARTED"
    );


    if (
        SD.begin(
            SD_CS,
            sdSPI
        )
    ) {

        sdOK = true;

        Serial.println(
            "[SD] READY"
        );


        uint8_t cardType =
            SD.cardType();


        if (
            cardType == CARD_NONE
        ) {

            Serial.println(
                "[SD] NO CARD"
            );

            sdOK = false;

        } else {

            Serial.print(
                "[SD] CARD TYPE: "
            );


            if (
                cardType == CARD_MMC
            ) {

                Serial.println(
                    "MMC"
                );

            } else if (
                cardType == CARD_SD
            ) {

                Serial.println(
                    "SDSC"
                );

            } else if (
                cardType == CARD_SDHC
            ) {

                Serial.println(
                    "SDHC"
                );

            } else {

                Serial.println(
                    "UNKNOWN"
                );
            }


            uint64_t cardSize =
                SD.cardSize() /
                (1024 * 1024);


            Serial.print(
                "[SD] SIZE: "
            );

            Serial.print(
                cardSize
            );

            Serial.println(
                " MB"
            );
        }

    } else {

        sdOK = false;

        Serial.println(
            "[SD] FAILED"
        );
    }


    // =================================================
    // BLE
    // =================================================

    BLEDevice::init(
        "SPARK_HUB"
    );


    bleScan =
        BLEDevice::getScan();


    bleScan->setAdvertisedDeviceCallbacks(
        new BLECallbacks()
    );


    bleScan->setActiveScan(
        true
    );


    bleScan->setInterval(
        100
    );


    bleScan->setWindow(
        80
    );


    Serial.println(
        "[BLE] READY"
    );


    // =================================================
    // START COLLECTION
    // =================================================

    currentMode =
        COLLECTION;


    startCollection();


    drawMiniOLED();


    drawCollectionOLED();


    Serial.println(
        "[SPARK] COLLECTION MODE"
    );


    Serial.println(
        "[HUB] READY"
    );
}


// =====================================================
// MAIN LOOP
// =====================================================

void loop() {

    // -----------------------------------------------
    // Button
    // -----------------------------------------------

    checkButton();


    // -----------------------------------------------
    // GPS
    // -----------------------------------------------

    readGPS();


    // -----------------------------------------------
    // ESP8266
    // -----------------------------------------------

    readWiFi();


    // =================================================
    // COLLECTION MODE
    // =================================================

    if (
        currentMode == COLLECTION
    ) {

        // Reset BLE count

        resetBLE();


        // Scan for 1 second

        bleScan->start(
            1,
            false
        );


        // Process anything received during scan

        readGPS();

        readWiFi();


        // Clear BLE results

        bleScan->clearResults();


        // ---------------------------------------------
        // Save once per second
        // ---------------------------------------------

        if (
            millis() - lastLog >=
            LOG_INTERVAL
        ) {

            lastLog =
                millis();


            recordData();
        }


    }

    // =================================================
    // WEBSERVER MODE
    // =================================================

    else {

        server.handleClient();
    }


    // =================================================
    // OLED UPDATE
    // =================================================

    if (
        millis() - lastDisplay >=
        DISPLAY_INTERVAL
    ) {

        lastDisplay =
            millis();


        if (
            currentMode == COLLECTION
        ) {

            drawCollectionOLED();

        } else {

            drawWebServerOLED();
        }


        drawMiniOLED();
    }


    delay(5);
}