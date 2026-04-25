# Copier Arm — Full Project Documentation

---

## Table of Contents

1. [Introduction](#introduction)
2. [Problem Statement](#problem-statement)
3. [Project Goals & Objectives](#project-goals--objectives)
4. [System Architecture](#system-architecture)
5. [Hardware Components](#hardware-components)
6. [Communication Pipeline](#communication-pipeline)
7. [Code Breakdown — How It All Works Together](#code-breakdown--how-it-all-works-together)
   - [ESP32 — Controller (Sender)](#esp32--controller-sender)
   - [ESP8266 (NodeMCU) — Bridge (Receiver & Forwarder)](#esp8266-nodemcu--bridge-receiver--forwarder)
   - [Arduino — Actuator (Servo Driver)](#arduino--actuator-servo-driver)
8. [Data Flow — End to End](#data-flow--end-to-end)
9. [Key Design Decisions](#key-design-decisions)
10. [Project Phases](#project-phases)
11. [Finished Phases](#finished-phases)
12. [Results & Demo](#results--demo)
13. [Next Steps](#next-steps)

---

## Introduction

The **Copier Arm** is a real-time robotic arm replication system. It consists of two physical arms:
- A **Controller Arm** — moved by hand, equipped with potentiometers at each joint.
- A **Copier Arm** — driven by servo motors that mirror every movement of the controller in real time.

The system spans **three microcontrollers**: an ESP32, a NodeMCU (ESP8266), and an Arduino, each playing a distinct role in the communication chain. Wireless data travels from the ESP32 to the ESP8266 using **ESP-NOW**, and then from the ESP8266 to the Arduino over **Serial (UART)**, where it finally drives the physical servos.

The mechanical design is based on the open-source **SO-ARM100** robotic arm (by TheRobotStudio on GitHub).

---

## Problem Statement

Controlling a robotic arm naturally and intuitively — without joysticks, GUIs, or complex software — is a challenge in many entry-level robotics projects. Traditional setups require pre-programmed routines or specialized control software, which creates friction between the operator and the machine.

This project removes that barrier by allowing a human to physically move one arm with their hand, and have a second robotic arm replicate every motion wirelessly and in real time. No screen. No code at runtime. Just gesture.

---

## Project Goals & Objectives

- Read the angular position of **5 joints** from potentiometers mounted on the controller arm.
- Transmit joint data **wirelessly** using the ESP-NOW protocol with minimal latency.
- Bridge that wireless data into a **Serial signal** readable by an Arduino.
- Drive **5 servo motors** on the copier arm to match the received joint angles.
- Apply **motion smoothing** to produce fluid, non-jittery movement.
- Deliver a stable, reliable end-to-end system validated through live testing.

---

## System Architecture

```
┌─────────────────────────────┐
│       CONTROLLER ARM        │
│                             │
│  5 Potentiometers           │
│  (finger, base, J1, J2, J3) │
│          │                  │
│          ▼                  │
│       ESP32                 │
│  - Reads ADC (12-bit)       │
│  - Smooths readings         │
│  - Sends via ESP-NOW        │
└─────────────┬───────────────┘
              │
         ESP-NOW (WiFi)
         Channel 1
         Wireless Packet
              │
┌─────────────▼───────────────┐
│       ESP8266 (NodeMCU)     │
│  - Receives ESP-NOW packet  │
│  - Validates & constrains   │
│  - Forwards via Serial      │
│    [0xFF][F][B][J1][J2][J3] │
└─────────────┬───────────────┘
              │
         Serial (UART)
         9600 baud
         Binary Protocol
              │
┌─────────────▼───────────────┐
│          ARDUINO            │
│  - Parses Serial stream     │
│  - Applies EMA smoothing    │
│  - Writes to 5 Servos       │
│                             │
│       COPIER ARM            │
│  (mirrors controller arm)   │
└─────────────────────────────┘
```

---

## Hardware Components

| Component | Quantity | Role |
|---|---|---|
| ESP32 | 1 | Reads potentiometers, sends ESP-NOW |
| NodeMCU ESP8266 | 1 | Receives ESP-NOW, bridges to Arduino via Serial |
| Arduino (Uno/Nano) | 1 | Parses Serial data, drives servos |
| Servo SG90 | 3 | Light-duty joints on the copier arm |
| Servo MG90S | 2 | Heavy-duty joints (metal gear) |
| Potentiometer 10K | 5 | Joint angle sensing on controller arm |
| CD4067 Multiplexer | 1 | *(Phase 1 design — replaced by direct ADC on ESP32)* |
| ACS712 Current Sensor | 1 | Overcurrent protection |
| Mini 360 Buck Converter | 1 | Regulated power for servos |
| Jumper Wires | Various | Wiring |

---

## Communication Pipeline

The system uses a **3-stage pipeline**:

### Stage 1 — ESP32 → ESP8266 (Wireless, ESP-NOW)

ESP-NOW is a connectionless WiFi protocol developed by Espressif. It works without a router, operates on a fixed WiFi channel, and sends raw data packets directly to a peer's MAC address with very low latency (typically under 5ms). No handshakes, no TCP overhead.

- The ESP32 sends a packed struct of 5 integers (20 bytes) directly to the ESP8266's MAC address.
- Both devices are locked to **Channel 1** to ensure they stay in sync.
- The ESP8266 is set as a **slave** that listens for incoming packets.

### Stage 2 — ESP8266 → Arduino (Wired, Serial UART)

Once the ESP8266 receives a packet, it immediately forwards the data to the Arduino over Serial at **9600 baud** using a simple **binary framing protocol**:

```
[0xFF] [finger] [base] [J1] [J2] [J3]
```

- `0xFF` is a **sync byte** — it marks the start of every frame.
- The 5 bytes that follow are the angle values (0–180), one per joint.
- This keeps packets small (6 bytes total) and easy to parse.

### Stage 3 — Arduino Parses & Drives Servos

The Arduino reads the serial stream byte by byte, waits for the `0xFF` sync byte, collects the next 5 bytes, applies **exponential smoothing**, and writes the resulting angle to each servo.

---

## Code Breakdown — How It All Works Together

### ESP32 — Controller (Sender)

**File:** `esp32.ino`

#### What it does:
Reads 5 potentiometers using the ESP32's 12-bit ADC, maps the raw values (0–4095) to angles (0–180°), and sends them wirelessly to the ESP8266 via ESP-NOW whenever a joint changes by more than 2 degrees.

#### Key functions:

**`smoothRead(pin)`** — Averages 16 ADC readings to eliminate electrical noise:
```cpp
int smoothRead(int pin) {
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(pin);
    delayMicroseconds(50);
  }
  return sum / 16;
}
```

**`readAngle(pin)`** — Converts a smoothed ADC reading to a servo-compatible angle:
```cpp
int readAngle(int pin) {
  return map(smoothRead(pin), 0, 4095, 0, 180);
}
```

**`anyChanged()`** — Compares current readings vs. last sent values with a ±2° threshold to avoid spamming packets for noise:
```cpp
bool anyChanged(JointData &a, JointData &b, int threshold = 2) {
  return abs(a.finger - b.finger) > threshold || ...
}
```

**`JointData` struct** — The data packet sent wirelessly:
```cpp
typedef struct {
  int finger;
  int base;
  int j1;
  int j2;
  int j3;
} JointData;
```

#### Pin assignments:
| Joint | ESP32 Pin |
|---|---|
| Finger | GPIO 36 |
| Base | GPIO 39 |
| J1 | GPIO 34 |
| J2 | GPIO 35 |
| J3 | GPIO 32 |

> **Note:** Pins 34–39 on the ESP32 are input-only ADC pins — ideal for potentiometers.

#### Setup details:
- `analogReadResolution(12)` — Uses the ESP32's full 12-bit ADC (4096 steps vs Arduino's 1024).
- `esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE)` — Forces WiFi to Channel 1, matching the ESP8266.
- The receiver's MAC address is hardcoded: `{0xE0, 0x98, 0x06, 0xA9, 0x78, 0x9B}`.

---

### ESP8266 (NodeMCU) — Bridge (Receiver & Forwarder)

**File:** `esp8266.ino`

#### What it does:
Acts as a **wireless-to-serial bridge**. It receives the ESP-NOW packet from the ESP32, then immediately serializes the 5 joint angles into a binary frame and sends it to the Arduino over Serial.

#### Key callback:

**`OnDataRecv()`** — Fires automatically when an ESP-NOW packet arrives:
```cpp
void OnDataRecv(uint8_t *mac, uint8_t *data, uint8_t len) {
  memcpy(&received, data, sizeof(received));
  newData = true;
}
```

The `volatile bool newData` flag ensures safe handoff between the interrupt-driven callback and the main loop.

#### Serial framing in `loop()`:
```cpp
Serial.write(0xFF);                                      // Sync byte
Serial.write((uint8_t)constrain(received.finger, 0, 180));
Serial.write((uint8_t)constrain(received.base,   0, 180));
Serial.write((uint8_t)constrain(received.j1,     0, 180));
Serial.write((uint8_t)constrain(received.j2,     0, 180));
Serial.write((uint8_t)constrain(received.j3,     0, 180));
```

- `constrain()` ensures no value escapes the 0–180 range before casting to `uint8_t`.
- The sync byte `0xFF` is the protocol's frame delimiter — the Arduino will not process any byte until it sees this.

#### Setup details:
- `WiFi.mode(WIFI_STA)` + `WiFi.disconnect()` — Station mode without connecting to any network, required for ESP-NOW.
- `wifi_set_channel(1)` — Must match the ESP32's forced channel.
- `esp_now_set_self_role(ESP_NOW_ROLE_SLAVE)` — Declares this device as the receiver.
- Serial baud rate: **9600** — matching the Arduino.

---

### Arduino — Actuator (Servo Driver)

**File:** `arduino.ino`

#### What it does:
Parses the binary serial stream from the ESP8266, applies **Exponential Moving Average (EMA)** smoothing to each joint angle, and writes the smoothed values to 5 servo motors.

#### Serial parser (state machine):

The parser works as a simple **2-state machine**:

```
State 1 — UNSYNCED:
  Read bytes one at a time.
  If byte == 0xFF → switch to SYNCED state, reset buffer index.

State 2 — SYNCED:
  Collect bytes into buffer.
  If byte == 0xFF → a new frame started mid-stream (error recovery), reset.
  When 5 bytes collected → process the frame, return to UNSYNCED.
```

```cpp
if (!synced) {
  if (b == 0xFF) { synced = true; bufIdx = 0; }
  continue;
}
if (b == 0xFF) { bufIdx = 0; continue; }  // Error recovery
buf[bufIdx++] = b;
if (bufIdx == 5) { /* process */ synced = false; bufIdx = 0; }
```

#### EMA Smoothing (ALPHA = 0.2):

Exponential Moving Average is applied to each joint before writing to the servo:

```
new_smooth = ALPHA × raw_value + (1 - ALPHA) × previous_smooth
```

With `ALPHA = 0.2`, each new reading contributes only 20% to the output — the remaining 80% is carried from history. This makes motion gradual and fluid instead of snapping to every small change.

```cpp
#define ALPHA 0.2
sFinger = ALPHA * finger + (1.0 - ALPHA) * sFinger;
sBase   = ALPHA * base   + (1.0 - ALPHA) * sBase;
// ... etc
```

> Lower ALPHA = smoother but slower response.
> Higher ALPHA = faster response but more jitter.
> `0.2` was chosen as a balanced default.

#### Pin assignments:
| Joint | Arduino Pin |
|---|---|
| Base | D3 |
| J1 | D5 |
| J2 | D6 |
| J3 | D9 |
| Finger | D10 |

> Pins D5, D6, D9, D10 are PWM-capable on the Arduino Uno/Nano — required for servo control.

---

## Data Flow — End to End

```
USER moves controller arm
        │
        ▼
Potentiometer angle changes
        │
        ▼
ESP32 reads ADC (16-sample average)
        │
        ▼
Maps 0–4095 → 0–180°
        │
        ▼
anyChanged()? threshold ±2°
        │ YES
        ▼
ESP-NOW packet sent (20 bytes)
to ESP8266 MAC over Channel 1
        │
        ▼
ESP8266 OnDataRecv() fires
        │
        ▼
Data constrained to [0, 180]
        │
        ▼
Binary frame sent over Serial:
[0xFF][finger][base][J1][J2][J3]
        │
        ▼
Arduino parser detects 0xFF sync
        │
        ▼
5 bytes collected into buffer
        │
        ▼
EMA smoothing applied (α = 0.2)
        │
        ▼
servo.write() called for each joint
        │
        ▼
Copier arm moves to match controller
```

---

## Key Design Decisions

### Why ESP-NOW instead of WebSocket?
ESP-NOW has significantly lower latency than WebSocket over WiFi. It requires no router, no TCP handshake, and no network infrastructure — just two devices knowing each other's MAC address. For a real-time motion system, this matters.

### Why a 3-device setup (ESP32 + ESP8266 + Arduino)?
The ESP8266 cannot natively drive multiple servo motors reliably while also handling WiFi — it lacks the necessary PWM pins and processing headroom. The Arduino is purpose-built for servo control with stable PWM output. The split keeps each device doing what it's best at.

### Why the 0xFF binary framing protocol?
It's the simplest possible framing that still supports **error recovery**. If the stream gets corrupted, the Arduino just waits for the next `0xFF` sync byte and resumes — no hanging, no crash.

### Why 12-bit ADC on the ESP32?
The Arduino ADC is only 10-bit (1024 steps), which introduces more quantization noise when reading potentiometers. The ESP32's 12-bit ADC (4096 steps) gives finer resolution and, combined with the 16-sample averaging, produces very clean and stable angle readings.

### Why EMA smoothing (not step-increment smoothing)?
Step-increment smoothing (move ±1° per loop) adds a fixed delay proportional to distance. EMA smoothing is **proportional** — large movements are fast, small adjustments are gentle. This feels more natural for arm control.

---

## Project Phases

| Phase | Description | Status |
|---|---|---|
| 1 | Component sourcing | ✅ Done |
| 2 | 3D printing & mechanical assembly | 🔄 In Progress |
| 3 | Circuit wiring (both arms) | 🔄 In Progress |
| 4 | Firmware development (all 3 devices) | ✅ Done |
| 5 | Calibration & end-to-end testing | ⏳ Pending |
| 6 | Improvements (recording, dashboard) | ⏳ Future |

---

## Finished Phases

**Phase 1 — Component Sourcing:** All hardware components have been identified and ordered from local Egyptian electronics suppliers (fut-electronics, microohm-eg).

**Phase 4 — Firmware Development:** All three firmware files are complete and reviewed:
- `esp32.ino` — ADC reading, smoothing, ESP-NOW sender.
- `esp8266.ino` — ESP-NOW receiver, Serial bridge.
- `arduino.ino` — Serial parser, EMA smoothing, servo driver.

The full communication pipeline — from potentiometer reading through wireless transmission through serial framing to servo actuation — has been designed and coded end-to-end.

---

## Results & Demo

> 📹 *This section will be updated once the physical build is complete.*

A demonstration video will be recorded showing:
- The controller arm being moved manually across all 5 joints.
- The copier arm replicating each movement in real time.
- Serial monitor output from the ESP32 logging the joint angles being sent.
- Smooth, continuous motion with no visible jitter or lag.

---

## Next Steps

1. **Complete mechanical assembly** — finish 3D printing and mount all servos and potentiometers into the SO-ARM100 frames.
2. **Wire the circuits** — connect the ESP32 to the 5 potentiometers, and the Arduino to the 5 servos with the Buck Converter providing regulated power.
3. **Hardware calibration** — verify that the physical range of each potentiometer maps correctly to the servo's working range, and adjust the `map()` limits in the ESP32 code if needed.
4. **Latency measurement** — log timestamps on send and receive to measure real-world end-to-end delay.
5. **Tune ALPHA** — test different smoothing values (0.1 – 0.4) to find the best balance between responsiveness and smoothness.
6. **Add overcurrent protection** — integrate the ACS712 sensor into the Arduino code to halt servos on overload.
7. **Motion recording** — explore saving a sequence of joint angles to EEPROM and replaying it autonomously.
