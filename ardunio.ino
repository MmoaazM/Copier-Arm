#include <Servo.h>

// Servo pins
#define PIN_BASE    3
#define PIN_J1      5
#define PIN_J2      6
#define PIN_J3      9
#define PIN_FINGER  10

Servo servoBase;
Servo servoJ1;
Servo servoJ2;
Servo servoJ3;
Servo servoFinger;

// Motion smoothing
#define ALPHA 0.2

float sBase = 90;
float sJ1 = 90;
float sJ2 = 90;
float sJ3 = 90;
float sFinger = 90;

// Serial parser
uint8_t buf[6];
uint8_t bufIdx = 0;
bool synced = false;

void setup() {

  Serial.begin(9600);

  servoBase.attach(PIN_BASE);
  servoJ1.attach(PIN_J1);
  servoJ2.attach(PIN_J2);
  servoJ3.attach(PIN_J3);
  servoFinger.attach(PIN_FINGER);

  // Start centered
  servoBase.write(90);
  servoJ1.write(90);
  servoJ2.write(90);
  servoJ3.write(90);
  servoFinger.write(90);

  delay(5000);
}

void loop() {

  while (Serial.available()) {

    uint8_t b = Serial.read();

    if (!synced) {

      if (b == 0xFF) {
        synced = true;
        bufIdx = 0;
      }

      continue;
    }

    if (b == 0xFF) {
      bufIdx = 0;
      continue;
    }

    buf[bufIdx++] = b;

    if (bufIdx == 5) {

      synced = false;
      bufIdx = 0;

      uint8_t finger = buf[0];
      uint8_t base   = buf[1];
      uint8_t j1     = buf[2];
      uint8_t j2     = buf[3];
      uint8_t j3     = buf[4];

      // Smooth movement
      sFinger = ALPHA * finger + (1.0 - ALPHA) * sFinger;
      sBase   = ALPHA * base   + (1.0 - ALPHA) * sBase;
      sJ1     = ALPHA * j1     + (1.0 - ALPHA) * sJ1;
      sJ2     = ALPHA * j2     + (1.0 - ALPHA) * sJ2;
      sJ3     = ALPHA * j3     + (1.0 - ALPHA) * sJ3;

      servoFinger.write((int)sFinger);
      servoBase.write((int)sBase);
      servoJ1.write((int)sJ1);
      servoJ2.write((int)sJ2);
      servoJ3.write((int)sJ3);
    }
  }
}
