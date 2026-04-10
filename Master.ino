/*
* Master ESP 

* send the pots values to slave ESP 
* by 3 steps :-
  1- get pots read  <Line 74>
  2- map the analog values of the pots to angles 0 -> 180  <Line 85>
  3- send angles via web socket protocl to Slave ESP <Line 93>
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

//  Multiplexer Pins 
#define MUX_S0  D1
#define MUX_S1  D2
#define MUX_S2  D3
#define MUX_S3  D4
#define MUX_SIG A0

//  WiFi & Slave Config 
const char* SSID     = "Our WiFi name";
const char* PASS     = "Our WiFi password";
const char* SLAVE_IP = "192.168.1.100";  // update after slave boots
const int   WS_PORT  = 81;

//  Timing
unsigned long lastTime = 0;
const unsigned long DeltaTime = 25;  // 25ms interval

//  Data 
int rawValues[5];   // 0–1023 from each pot
int angles[5];      // 0–180 mapped angles

WebSocketsClient ws;
bool wsConnected = false;


// WebSocket Event Callback

void onWsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            wsConnected = true;
            Serial.println("Connected to Slave!");
            break;
        case WStype_DISCONNECTED:
            wsConnected = false;
            Serial.println("Disconnected — retrying...");
            break;
        case WStype_TEXT:
            Serial.printf("Slave says: %s\n", payload);
            break;
    }
}


// Step 1 — Select MUX Channel
// channels from 0 to 4 → writes 4 bits to S0-S3

void setMuxChannel(int ch) {
    digitalWrite(MUX_S0, (ch >> 0) & 1);
    digitalWrite(MUX_S1, (ch >> 1) & 1);
    digitalWrite(MUX_S2, (ch >> 2) & 1);
    digitalWrite(MUX_S3, (ch >> 3) & 1);
    delayMicroseconds(100); // let MUX settle before reading
}

// Step 2 — Read All 5 Potentiometers

void readAllPots() {
    for (int i = 0; i < 5; i++) {
        setMuxChannel(i);
        rawValues[i] = analogRead(MUX_SIG); // 0..1023
    }
}


// Step 3 — Map Raw Values to Servo Angles

void mapToAngles() {
    for (int i = 0; i < 5; i++) {
        angles[i] = map(rawValues[i], 0, 1023, 0, 180);
    }
}

// Step 4 — Send Angles to Slave via WebSocket

void sendToSlave() {
    if (!wsConnected) return;

    // build JSON: {"a":[90,45,120,30,60]}
    StaticJsonDocument<128> doc;
    JsonArray arr = doc.createNestedArray("a");
    for (int i = 0; i < 5; i++) arr.add(angles[i]);

    char buffer[128];
    serializeJson(doc, buffer);

    ws.sendTXT(buffer);
}


void setup() {
    Serial.begin(115200);

    // MUX select pins as output
    pinMode(MUX_S0, OUTPUT);
    pinMode(MUX_S1, OUTPUT);
    pinMode(MUX_S2, OUTPUT);
    pinMode(MUX_S3, OUTPUT);

    // Connect to WiFi
    Serial.print("Connecting to WiFi");
    WiFi.begin(SSID, PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500); Serial.print(".");
    }
    Serial.println("\nWiFi OK — IP: " + WiFi.localIP().toString()); // to know the IP then we can delete it 

    // Connect WebSocket to Slave
    ws.begin(SLAVE_IP, WS_PORT, "/");
    ws.onEvent(onWsEvent);
    ws.setReconnectInterval(1000); // retry every 1s if disconnected
}

void loop() {
    ws.loop(); // must be first — keeps WebSocket alive

    unsigned long now = millis();
    if (now - lastTime < DeltaTime) return; 
    lastTime = now;

    readAllPots();    
    mapToAngles();    
    sendToSlave();    

    // just for debugging can be deleted if we are sure everything is fine
    Serial.print("Angles: ");
    for (int i = 0; i < 5; i++) {
        Serial.printf("[%d]=%d° ", i, angles[i]);
    }
    Serial.println();
}
