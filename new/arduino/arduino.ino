// ============================================================
//  COPIER ARM — Single Arduino Uno
//  Pots → read angles → write servos directly
//  Button (pin 7): MIRROR → RECORD → REPLAY → MIRROR
//  Max recording: ~350 frames ≈ 17 seconds
// ============================================================

#include <Servo.h>

// ── Potentiometer pins ──
#define POT_FINGER  A0
#define POT_BASE    A1
#define POT_J1      A2
#define POT_J2      A3
#define POT_J3      A4

// ── Servo pins (all PWM) ──
#define SRV_FINGER   3
#define SRV_BASE     5
#define SRV_J1       6
#define SRV_J2       9
#define SRV_J3      10

// ── Button ──
#define BTN_PIN      7

// ── Recording ──
#define MAX_FRAMES  350   // 350 × 5 bytes = 1750 bytes (fits in Uno's 2KB SRAM)

Servo servoFinger, servoBase, servoJ1, servoJ2, servoJ3;

struct JointData { byte finger, base, j1, j2, j3; };

enum Mode { MIRROR, RECORD, REPLAY };
Mode currentMode = MIRROR;

JointData recording[MAX_FRAMES];
int  frameCount  = 0;
int  replayIndex = 0;

unsigned long lastTick    = 0;
unsigned long lastDebounce = 0;
bool lastBtnState = HIGH;

#define TICK_MS     50
#define DEBOUNCE_MS 50

// ────────────────────────────────────────────
int smoothRead(int pin) {
  long sum = 0;
  for (int i = 0; i < 8; i++) {
    sum += analogRead(pin);
    delayMicroseconds(100);
  }
  return sum / 8;q
}

byte readAngle(int pin) {
  return (byte) map(smoothRead(pin), 0, 1023, 0, 180);
}

JointData readPots() {
  return { readAngle(POT_FINGER), readAngle(POT_BASE),
           readAngle(POT_J1),    readAngle(POT_J2), readAngle(POT_J3) };
}

void applyServos(JointData &d) {
  servoFinger.write(d.finger);
  servoBase.write(d.base);
  servoJ1.write(d.j1);
  servoJ2.write(d.j2);
  servoJ3.write(d.j3);
}

// ────────────────────────────────────────────
void handleButton() {
  bool state = digitalRead(BTN_PIN);
  if (state != lastBtnState) lastDebounce = millis();

  if ((millis() - lastDebounce) > DEBOUNCE_MS
      && state == LOW && lastBtnState == HIGH) {

    if      (currentMode == MIRROR) { currentMode = RECORD; frameCount = 0; }
    else if (currentMode == RECORD) { currentMode = REPLAY; replayIndex = 0; }
    else                            { currentMode = MIRROR; }
  }
  lastBtnState = state;
}

// ────────────────────────────────────────────
void setup() {
  servoFinger.attach(SRV_FINGER);
  servoBase.attach(SRV_BASE);
  servoJ1.attach(SRV_J1);
  servoJ2.attach(SRV_J2);
  servoJ3.attach(SRV_J3);
  pinMode(BTN_PIN, INPUT_PULLUP);
}

void loop() {
  handleButton();

  if (millis() - lastTick < TICK_MS) return;
  lastTick = millis();

  if (currentMode == MIRROR || currentMode == RECORD) {
    JointData d = readPots();
    applyServos(d);
    if (currentMode == RECORD && frameCount < MAX_FRAMES)
      recording[frameCount++] = d;

  } else {  // REPLAY
    if (frameCount > 0) {
      applyServos(recording[replayIndex]);
      replayIndex = (replayIndex + 1) % frameCount;
    }
  }
}