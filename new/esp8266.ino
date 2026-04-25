#include <ESP8266WiFi.h>
#include <espnow.h>

extern "C" {
  #include <user_interface.h>
}

typedef struct {
  int finger;
  int base;
  int j1;
  int j2;
  int j3;
} JointData;

JointData received;
volatile bool newData = false;

void OnDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {

  memcpy(&received, data, sizeof(received));
  newData = true;
}

void setup() {

  Serial.begin(9600);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != 0) {
    while (true);
  }

  wifi_set_channel(1);

  esp_now_set_self_role(ESP_NOW_ROLE_SLAVE);
  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {

  if (!newData) return;

  newData = false;

  Serial.write(0xFF);

  Serial.write((uint8_t)constrain(received.finger, 0, 180));
  Serial.write((uint8_t)constrain(received.base,   0, 180));
  Serial.write((uint8_t)constrain(received.j1,     0, 180));
  Serial.write((uint8_t)constrain(received.j2,     0, 180));
  Serial.write((uint8_t)constrain(received.j3,     0, 180));
}
