/*
   Slave ESP8266
   Receives angles from master and moves servos.

   Master sends JSON like:
   {"a":[a0,a1,a2,a3,a4]}
*/

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>
#include <Servo.h>

// WiFi hotspot info
const char* SSID = "Infinix HOT 60 Pro+";
const char* PASS = "jojojojo";

// same port used by master
const int WS_PORT = 81;

// Servo pin connections
// I wired them in this order
const int SERVO_PINS[5] = { D5, D6, D7, D8, D0 };
const int NUM_SERVOS    = 5;

// smoothing settings
// smaller step = smoother movement
const float MAX_STEP  = 3.0f;
const int   SMOOTH_MS = 15;


// servo objects
Servo servos[NUM_SERVOS];

// current and target angles
float currentAngle[NUM_SERVOS];
float targetAngle[NUM_SERVOS];

// WebSocket server
WebSocketsServer wsServer(WS_PORT);

// timer for smoothing loop
unsigned long lastSmoothTime = 0;


// keeps values between limits
float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}


// handles WebSocket events
void onWsEvent(uint8_t clientNum, WStype_t type,
               uint8_t* payload, size_t length)
{
    switch (type) {

        // when master connects
        case WStype_CONNECTED: {
            IPAddress ip = wsServer.remoteIP(clientNum);
            Serial.printf("Client connected from %s\n",
                          ip.toString().c_str());

            // send ready message
            wsServer.sendTXT(clientNum,
                             "{\"status\":\"slave_ready\"}");
            break;
        }

        // when master disconnects
        case WStype_DISCONNECTED:
            Serial.println("Master disconnected");
            break;


        // when angles are received
        case WStype_TEXT: {

            StaticJsonDocument<128> doc;

            // read JSON data
            DeserializationError err =
                deserializeJson(doc, payload, length);

            if (err) {
                Serial.println("JSON parse error");
                break;
            }

            // check if array exists
            if (!doc.containsKey("a") ||
                !doc["a"].is<JsonArray>()) {

                Serial.println("Invalid data");
                break;
            }

            JsonArray arr =
                doc["a"].as<JsonArray>();

            // check correct number of angles
            if (arr.size() < NUM_SERVOS) {

                Serial.println("Not enough values");
                break;
            }

            // update target angles
            Serial.print("Targets: ");

            for (int i = 0; i < NUM_SERVOS; i++) {

                float angle =
                    (float)arr[i].as<int>();

                targetAngle[i] =
                    clampf(angle, 0.0f, 180.0f);

                Serial.printf("[%d]=%.0f ",
                              i, targetAngle[i]);
            }

            Serial.println();
            break;
        }

        default:
            break;
    }
}


// moves servos slowly toward target
void smoothServos() {

    for (int i = 0; i < NUM_SERVOS; i++) {

        float diff =
            targetAngle[i] - currentAngle[i];

        // if already close enough
        if (abs(diff) < 0.5f) {

            currentAngle[i] = targetAngle[i];
            continue;
        }

        // move step by step
        float step =
            clampf(diff, -MAX_STEP, MAX_STEP);

        currentAngle[i] += step;

        currentAngle[i] =
            clampf(currentAngle[i], 0.0f, 180.0f);

        // send to servo
        servos[i].write(
            (int)currentAngle[i]);
    }
}


// runs once
void setup() {

    Serial.begin(115200);

    Serial.println("Slave starting");


    // attach servos and center them
    for (int i = 0; i < NUM_SERVOS; i++) {

        servos[i].attach(SERVO_PINS[i]);

        currentAngle[i] = 90.0f;
        targetAngle[i]  = 90.0f;

        servos[i].write(90);
    }

    Serial.println("Servos ready");


    // connect to WiFi
    Serial.printf("Connecting to %s ",
                  SSID);

    WiFi.mode(WIFI_STA);

    // static IP setup
    IPAddress staticIP(10, 94, 114, 100);
    IPAddress gateway(10, 94, 114, 192);
    IPAddress subnet(255, 255, 255, 0);

    WiFi.config(staticIP, gateway, subnet);

    WiFi.begin(SSID, PASS);


    int attempts = 0;

    // wait until connected
    while (WiFi.status() != WL_CONNECTED) {

        delay(500);
        Serial.print(".");

        attempts++;

        if (attempts > 40) {

            Serial.println("WiFi failed");
            delay(2000);
            ESP.restart();
        }
    }

    Serial.println();
    Serial.print("Connected IP: ");
    Serial.println(WiFi.localIP());


    // start WebSocket server
    wsServer.begin();
    wsServer.onEvent(onWsEvent);

    Serial.println("Server ready");
}


// main loop
void loop() {

    // keep WebSocket running
    wsServer.loop();

    // run smoothing every few ms
    unsigned long now = millis();

    if (now - lastSmoothTime >=
        (unsigned long)SMOOTH_MS) {

        lastSmoothTime = now;

        smoothServos();
    }
}
