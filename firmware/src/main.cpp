// Predictible Putter — Slice 1: sensor bring-up over USB serial.
//
// Goal (per CLAUDE.md build order): stream raw 1 kHz IMU samples over serial so we
// can (a) prove the MPU-6050 works and (b) capture a REAL putt impact to set the
// accel/gyro ranges and impact trigger threshold from data — closing the empirical
// open items in Spec 0001 (Q2/Q3) and Spec 0002 rather than guessing them.
//
// This slice does NOT do BLE, buffering, or impact detection yet (that is Slice 2).
// It just prints CSV: t_us,ax,ay,az,gx,gy,gz  (raw int16 counts, Spec 0001 §4.4).

#include <Arduino.h>
#include <Wire.h>

#include "Mpu6050.h"

namespace {
constexpr uint32_t kSampleRateHz = 1000;
constexpr uint32_t kSamplePeriodUs = 1000000UL / kSampleRateHz;  // 1000 µs
constexpr int kSdaPin = 21;  // ESP32 DevKit default I2C pins
constexpr int kSclPin = 22;
constexpr uint32_t kI2cClockHz = 400000;  // fast mode (Spec 0001 §4.1)

Mpu6050 imu;
uint32_t next_tick_us = 0;

// Bring-up diagnostic: probe every I2C address and report who answers. Lets us tell
// "sensor dead / not wired" (nothing found) apart from an address/AD0 problem
// (found at 0x69 instead of 0x68). Remove once bring-up is done.
void scanI2cBus() {
  Serial.println("# I2C scan:");
  int found = 0;
  for (uint8_t addr = 1; addr < 127; ++addr) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf("#   device at 0x%02X\n", addr);
      ++found;
    }
  }
  if (found == 0) {
    Serial.println("#   (nothing responded — check SDA/SCL wiring & common ground)");
  }
}
}  // namespace

void setup() {
  Serial.begin(PUTTER_SERIAL_BAUD);
  while (!Serial) { /* wait for USB serial */ }
  delay(200);

  Wire.begin(kSdaPin, kSclPin);
  Wire.setClock(kI2cClockHz);

  scanI2cBus();

  if (!imu.begin()) {
    // Halt loudly: nothing downstream is meaningful without the sensor.
    while (true) {
      Serial.println("# ERROR: MPU-6050 not found (check wiring/address 0x68)");
      delay(1000);
    }
  }

  // Self-describing header so a recording carries its own config (ranges, rate).
  Serial.println("# putter-slice1 v1");
  Serial.printf("# accel_fs_g=%.0f gyro_fs_dps=%.0f dlpf=widest rate_target_hz=%lu\n",
                Mpu6050::kAccelFsG, Mpu6050::kGyroFsDps,
                static_cast<unsigned long>(kSampleRateHz));
  Serial.println("t_us,ax,ay,az,gx,gy,gz");

  next_tick_us = micros();
}

void loop() {
  // Fixed-rate pacing. We stamp each row with the actual micros() read time, so the
  // true sample rate is measured from the data, never assumed.
  const uint32_t now = micros();
  if (static_cast<int32_t>(now - next_tick_us) < 0) return;
  next_tick_us += kSamplePeriodUs;

  Mpu6050::Sample s;
  if (!imu.read(s)) {
    Serial.println("# WARN: read failed");
    return;
  }

  // Compact CSV keeps 1 kHz within the serial budget at 921600 baud.
  Serial.printf("%lu,%d,%d,%d,%d,%d,%d\n", static_cast<unsigned long>(now),
                s.ax, s.ay, s.az, s.gx, s.gy, s.gz);
}
