#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// =====================================================
// ENCODER
// =====================================================

#define ENCODER_CLK 25
#define ENCODER_DT  26
#define ENCODER_SW  27


// =====================================================
// WIFI / WEB SERVER
// =====================================================

const char* AP_SSID = "ESP32-Website";
const char* AP_PASSWORD = "12345678";

WebServer server(80);
DNSServer dnsServer;

#define DNS_PORT 53


// =====================================================
// MODES
// =====================================================

enum Mode {
    WEB_MODE,
    SNIFFER_MODE
};

Mode currentMode = WEB_MODE;


// =====================================================
// ENCODER BUTTON STATE
// =====================================================

bool lastButtonState = HIGH;
unsigned long lastButtonPress = 0;

const unsigned long DEBOUNCE_TIME = 300;


// =====================================================
// WEBSITE
// =====================================================

void handleRoot() {

    server.send(200, "text/html", R"rawliteral(

<!DOCTYPE html>

<html>

<head>

<meta name="viewport"
      content="width=device-width, initial-scale=1">

<title>ESP32 Control</title>

<style>

body {
    margin: 0;
    padding: 0;

    font-family: Arial, sans-serif;

    background: #111;
    color: white;

    display: flex;
    justify-content: center;
    align-items: center;

    height: 100vh;
}

.container {
    text-align: center;
}

h1 {
    font-size: 40px;
}

.status {
    font-size: 22px;
    margin-top: 20px;
}

</style>

</head>

<body>

<div class="container">

<h1>ESP32</h1>

<div class="status">
WEB SERVER MODE
</div>

<p>
The ESP32 is currently hosting this website.
</p>

<p>
Press the rotary encoder to switch modes.
</p>

</div>

</body>

</html>

)rawliteral");
}


// =====================================================
// START WEB SERVER
// =====================================================

void startWebServer() {

    Serial.println();
    Serial.println("==============================");
    Serial.println("STARTING WEB SERVER");
    Serial.println("==============================");

    // Turn WiFi on
    WiFi.mode(WIFI_AP);

    delay(100);

    // Create access point
    bool result = WiFi.softAP(
        AP_SSID,
        AP_PASSWORD
    );

    if (!result) {

        Serial.println("ERROR: Failed to start AP!");

        return;
    }

    IPAddress ip = WiFi.softAPIP();

    Serial.print("WiFi SSID: ");
    Serial.println(AP_SSID);

    Serial.print("Password: ");
    Serial.println(AP_PASSWORD);

    Serial.print("IP Address: ");
    Serial.println(ip);


    // Start DNS
    dnsServer.start(
        DNS_PORT,
        "*",
        ip
    );


    // Website
    server.on(
        "/",
        HTTP_GET,
        handleRoot
    );


    // Start HTTP server
    server.begin();

    Serial.println("HTTP server started.");
    Serial.println("==============================");
}


// =====================================================
// STOP WEB SERVER COMPLETELY
// =====================================================

void stopWebServer() {

    Serial.println();
    Serial.println("==============================");
    Serial.println("STOPPING WEB SERVER");
    Serial.println("==============================");


    // Stop HTTP server
    server.stop();

    Serial.println("HTTP server stopped.");


    // Stop DNS server
    dnsServer.stop();

    Serial.println("DNS server stopped.");


    // Disconnect all AP clients
    WiFi.softAPdisconnect(true);

    Serial.println("Access point disconnected.");


    // Disconnect WiFi completely
    WiFi.disconnect(true, true);

    Serial.println("WiFi disconnected.");


    // Turn WiFi hardware off
    WiFi.mode(WIFI_OFF);

    delay(500);


    Serial.println("WiFi hardware OFF.");
    Serial.println("==============================");
}


// =====================================================
// START BLUETOOTH SNIFFER
// =====================================================

void startSniffer() {

    Serial.println();
    Serial.println("==============================");
    Serial.println("STARTING BLUETOOTH SNIFFER");
    Serial.println("==============================");

    /*
       YOUR BLUETOOTH SNIFFER CODE
       WILL GO HERE.
    */

    Serial.println("Sniffer running.");
}


// =====================================================
// STOP BLUETOOTH SNIFFER
// =====================================================

void stopSniffer() {

    Serial.println();
    Serial.println("==============================");
    Serial.println("STOPPING BLUETOOTH SNIFFER");
    Serial.println("==============================");

    /*
       YOUR BLUETOOTH SNIFFER
       SHUTDOWN CODE WILL GO HERE.
    */

    Serial.println("Sniffer stopped.");
}


// =====================================================
// SWITCH WEB -> SNIFFER
// =====================================================

void switchToSniffer() {

    Serial.println();
    Serial.println("******** MODE CHANGE ********");
    Serial.println("WEB SERVER -> SNIFFER");


    // Completely kill web server
    stopWebServer();


    // Change mode
    currentMode = SNIFFER_MODE;


    // Start sniffer
    startSniffer();


    Serial.println("******** SNIFFER MODE ********");
}


// =====================================================
// SWITCH SNIFFER -> WEB
// =====================================================

void switchToWeb() {

    Serial.println();
    Serial.println("******** MODE CHANGE ********");
    Serial.println("SNIFFER -> WEB SERVER");


    // Stop Bluetooth
    stopSniffer();


    delay(200);


    // Change mode
    currentMode = WEB_MODE;


    // Start WiFi + website
    startWebServer();


    Serial.println("******** WEB MODE ********");
}


// =====================================================
// ENCODER BUTTON
// =====================================================

void checkEncoderButton() {

    bool buttonState = digitalRead(ENCODER_SW);


    // Detect HIGH -> LOW transition
    if (
        lastButtonState == HIGH &&
        buttonState == LOW
    ) {

        unsigned long now = millis();


        // Debounce
        if (
            now - lastButtonPress >
            DEBOUNCE_TIME
        ) {

            lastButtonPress = now;


            if (currentMode == WEB_MODE) {

                switchToSniffer();

            }

            else {

                switchToWeb();

            }
        }
    }


    lastButtonState = buttonState;
}


// =====================================================
// SETUP
// =====================================================

void setup() {

    Serial.begin(115200);

    delay(1000);


    Serial.println();
    Serial.println();
    Serial.println("==============================");
    Serial.println("ESP32 DUAL MODE TEST");
    Serial.println("==============================");


    // Encoder
    pinMode(
        ENCODER_CLK,
        INPUT_PULLUP
    );

    pinMode(
        ENCODER_DT,
        INPUT_PULLUP
    );

    pinMode(
        ENCODER_SW,
        INPUT_PULLUP
    );


    Serial.println("Encoder initialized.");


    // Start in web mode
    currentMode = WEB_MODE;

    startWebServer();


    Serial.println();
    Serial.println("DEVICE READY.");
    Serial.println();
}


// =====================================================
// LOOP
// =====================================================

void loop() {

    // Always check encoder
    checkEncoderButton();


    // WEB MODE
    if (currentMode == WEB_MODE) {

        dnsServer.processNextRequest();

        server.handleClient();
    }


    // SNIFFER MODE
    else if (currentMode == SNIFFER_MODE) {

        /*
           YOUR BLUETOOTH SNIFFER
           PROCESSING GOES HERE.
        */

    }
}