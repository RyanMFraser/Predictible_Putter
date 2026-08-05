# Spec 0001 — System Architecture & Data Flow

- **Status:** AGREED (v1) — capture model and desktop stack ratified; remaining items in §6 are
  bring-up tuning (Q2, Q3) or a pre-firmware follow-up (Q5), none of which change the data contract.
- **Owner:** Ryan
- **Related:** `CLAUDE.md` §6 (architecture), §7 (v1 scope)
- **Supersedes:** none

> Legend: **[PROPOSED — ratify]** = a concrete choice put forward for agreement; change freely.
> **[OPEN]** = genuinely undecided, needs a decision before it affects code.

---

## 1. Context & goal
Define the end-to-end system: how a putting stroke goes from the sensor on the putter head to
usable numbers in a desktop program. This spec fixes the **module boundaries** and the **data
contract** between them, so firmware and desktop software can be built independently against an
agreed interface. It does **not** define the force/rotation math (that is a later analysis spec)
— only what data is captured and how it is delivered.

## 2. Requirements
Functional:
- R1. The system shall capture 6-axis IMU data (accelerometer + gyroscope) from the putter head
  during a putting stroke.
- R2. The system shall deliver, for each detected stroke, a time-series window covering the
  stroke through impact, to a desktop program over BLE.
- R3. Each delivered sample shall carry a device timestamp precise enough to resolve the impact
  event (see §4.1).
- R4. The desktop program shall receive stroke windows and make them available to an analysis
  layer, exposed via a CLI for v1.
- R5. The system shall operate untethered (battery + BLE) for indoor practice.

Non-functional:
- R6. The firmware/transport/analysis/UI layers shall be separable, so a GUI can replace the CLI
  without changing analysis code, and analysis can be tested on recorded data without hardware.
- R7. Captured stroke data shall be recordable to disk so analysis can be developed/replayed
  offline without the device (supports R6 and spec-driven testing).
- R8. No data loss within a delivered stroke window: samples arrive in order, gaps detectable.

## 3. Assumptions & constraints
- A1. Hardware is per `CLAUDE.md` §3: ESP32-WROOM-32 DevKit, MPU-6050 in the mallet rear cavity.
- A2. A putt is a low-speed event (head speed on the order of ~0.5–2 m/s); the impact itself is a
  short transient of a few milliseconds. **[PROPOSED — ratify]** Values to be confirmed empirically.
- A3. BLE bandwidth — not continuous-full-rate — is the binding transport constraint. Practical
  sustained BLE throughput on this ESP32 is well below the ~128 kbps that continuous 1 kHz
  6-axis streaming would require, so continuous raw streaming is assumed **not** viable.
- A4. The sensor is offset behind the face center; delivered data is raw, and geometric
  correction for that lever arm is the analysis layer's responsibility, not firmware's.

## 4. Design

### 4.1 Sampling (firmware, on ESP-32)
- **[PROPOSED — ratify]** Sample both accel and gyro at **1 kHz** (1 ms period).
- **[PROPOSED — ratify]** I²C bus at **400 kHz** (fast mode) to sustain 1 kHz 6-axis reads.
- **[PROPOSED — ratify]** Full-scale ranges — start at **accel ±16 g** (headroom for the impact
  shock transient so the spike is not clipped) and **gyro ±500 °/s**. These are starting points
  to validate during bring-up; if the impact spike clips, or if resolution is poor, revisit.
- **[PROPOSED — ratify]** Timestamp each sample with the ESP-32 microsecond clock (`esp_timer`),
  delivered as microseconds since capture-window start (µs resolution, easily enough for R3).

### 4.2 Stroke capture model — windowed, impact-triggered [DECIDED]
Because of A3, the firmware cannot stream everything. **Decided model (window around impact only):**
- Firmware samples continuously at 1 kHz into a **ring buffer** in RAM.
- A **lightweight on-device trigger** (a simple threshold on acceleration/jerk magnitude) marks a
  candidate impact. This is a *trigger only*, not analysis — all force/rotation computation stays
  off-device (honors CLAUDE.md's "analysis is off-device").
- On trigger, firmware transmits a **window** around impact over BLE — **[PROPOSED — ratify]**
  ~**300 ms pre** + **300 ms post** impact (≈600 samples ≈ ~10 KB), a size BLE can move as a burst.
- **Decision:** v1 captures **only the window around impact** — sufficient for force + face
  rotation at impact. The reduced-rate full-stroke path (for tempo/backswing) is deferred to a
  later version, not built now.
- **[OPEN]** Trigger threshold value and pre/post window durations — to be set empirically during
  bring-up, then recorded here.

### 4.3 Transport (BLE)
- **[PROPOSED — ratify]** ESP-32 acts as **BLE peripheral (GATT server)**; the desktop is central.
- **[PROPOSED — ratify]** Custom GATT service with characteristics for: (a) streaming stroke-window
  sample notifications, (b) control (start/stop, set trigger threshold), (c) device status/battery.
- **[PROPOSED — ratify]** Request an enlarged **ATT MTU** and packed binary samples to maximize
  throughput per notification. Exact packet layout defined in §4.4.

### 4.4 Data contract (the interface both sides build against) [PROPOSED — ratify]
One **sample** (packed binary, little-endian):

| Field | Type | Bytes | Notes |
|------|------|-------|-------|
| `t_us` | uint32 | 4 | microseconds since window start |
| `ax,ay,az` | int16 ×3 | 6 | raw accelerometer counts (scale per configured range) |
| `gx,gy,gz` | int16 ×3 | 6 | raw gyroscope counts (scale per configured range) |

= **16 bytes/sample**. A **stroke window** = a header (sample count, sample rate, accel range,
gyro range, sequence id) followed by N samples. Raw counts are sent (not physical units) to keep
packets small and keep unit conversion + calibration in one place (the analysis layer).

### 4.5 Desktop software layering (honors R6)
```
BLE client  ->  Recorder/Replay  ->  Analysis  ->  CLI (v1)  [-> GUI later]
(transport)     (raw window I/O)     (force,        (present)
                 to/from disk         rotation)
```
- **BLE client:** connects, subscribes, reassembles stroke windows into a validated struct.
- **Recorder/Replay:** persists raw stroke windows to disk (R7) and can feed them back to Analysis
  with no hardware present — this is how analysis is developed and tested.
- **Analysis:** pure functions over a stroke window → results. No BLE/CLI knowledge. Testable.
- **CLI:** the v1 user surface. A GUI later consumes the same Analysis layer.
- **[DECIDED] Desktop language/stack: Python.** BLE via `bleak` (cross-platform, async), numeric
  and signal work via numpy/scipy, plotting via matplotlib. Chosen for fast iteration on sensor
  analysis. A later GUI can be Python (e.g. a web/desktop UI) reusing the same Analysis layer.

## 5. Acceptance criteria
- AC1. A documented GATT service + the §4.4 binary sample/window layout exist and both firmware and
  desktop build against them.
- AC2. With the device mounted and connected, making a putting stroke causes exactly one stroke
  window to be delivered to the desktop, containing samples spanning before and after impact.
- AC3. A delivered window can be written to disk and later replayed into the Analysis layer with no
  device connected, producing identical input to the online path (R6/R7).
- AC4. Samples within a window are in monotonic timestamp order with no dropped samples, or any gap
  is detectable from the sequence/timestamps (R8).
- AC5. The impact transient is visible and **not clipped** in the accelerometer channel at the
  chosen full-scale range (validates §4.1 range choice).

## 6. Open questions
Closed:
- Q1 (§4.2): **CLOSED** — v1 captures the window around impact only; full-stroke path deferred.
- Q4 (§4.5): **CLOSED** — desktop stack is Python (bleak / numpy / scipy / matplotlib).

Still open (do not block the data contract; close before the code they affect):
- Q2 (§4.2): Impact trigger threshold + pre/post window durations — set empirically at bring-up,
  then recorded here.
- Q3 (§4.1): Confirm full-scale ranges after observing a real impact (clipping? resolution?) —
  bring-up tuning.
- Q5: **CLOSED** — firmware uses **PlatformIO + Arduino-ESP32** framework.

## 7. Out of scope (this spec)
- The force/speed and rotation math and units (later analysis spec).
- Training/feedback semantics: target values, tolerance bands, streaks (later spec).
- Any GUI. Outdoor/on-green operation.
