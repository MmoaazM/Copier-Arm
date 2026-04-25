#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>  // ← needed for esp_wifi_set_channel

uint8_t receiverMAC[] = {0xE0, 0x98, 0x06, 0xA9, 0x78, 0x9B};

#define PIN_FINGER 36
#define PIN_BASE   39
#define PIN_J1     34
#define PIN_J2     35
#define PIN_J3     32

typedef struct {
  int finger;
  int base;
  int j1;
  int j2;
  int j3;
} JointData;

JointData toSend;
JointData lastSent;

int smoothRead(int pin) {
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(pin);
    delayMicroseconds(50);
  }
  return sum / 16;
}

int readAngle(int pin) {
  return map(smoothRead(pin), 0, 4095, 0, 180);
}

bool anyChanged(JointData &a, JointData &b, int threshold = 2) {
  return abs(a.finger - b.finger) > threshold
      || abs(a.base   - b.base)   > threshold
      || abs(a.j1     - b.j1)     > threshold
      || abs(a.j2     - b.j2)     > threshold
      || abs(a.j3     - b.j3)     > threshold;
}

void OnDataSent(const wifi_tx_info_t *info, esp_now_send_status_t status) { }

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    while (true);
  }

  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);  // ← after init, not before

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peer = {};
  memcpy(peer.peer_addr, receiverMAC, 6);
  peer.channel = 1;      // ← match the forced channel
  peer.encrypt = false;

  if (esp_now_add_peer(&peer) != ESP_OK) {
    Serial.println("Add peer failed");
    while (true);
  }

  Serial.println("Sender ready");
}

void loop() {
  toSend.finger = readAngle(PIN_FINGER);
  toSend.base   = readAngle(PIN_BASE);
  toSend.j1     = readAngle(PIN_J1);
  toSend.j2     = readAngle(PIN_J2);
  toSend.j3     = readAngle(PIN_J3);

  if (anyChanged(toSend, lastSent)) {
    esp_now_send(receiverMAC, (uint8_t *)&toSend, sizeof(toSend));
    lastSent = toSend;
    Serial.printf("F=%d B=%d J1=%d J2=%d J3=%d\n",
      toSend.finger, toSend.base,
      toSend.j1, toSend.j2, toSend.j3);
  }

  delay(50);
}
