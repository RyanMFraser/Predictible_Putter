// Mpu6050 — minimal register-level driver for the InvenSense MPU-6050.
//
// Deliberately thin: it reads *raw int16 counts* (no float conversion), which is
// exactly the form the Spec 0001 §4.4 data contract puts on the wire. Unit
// conversion and calibration live off-device in the analysis layer (Spec 0002),
// so the firmware stays dumb and fast.
#pragma once

#include <Arduino.h>
#include <Wire.h>

class Mpu6050 {
public:
  // One raw IMU reading. Counts scale per the configured full-scale ranges
  // (see kAccelFsG / kGyroFsDps below). Temperature is intentionally skipped.
  struct Sample {
    int16_t ax, ay, az;  // accelerometer counts
    int16_t gx, gy, gz;  // gyroscope counts
  };

  // Configured full-scale ranges (Spec 0001 §4.1 PROPOSED values).
  // Exposed so a reader can convert counts -> physical units if it ever needs to.
  static constexpr float kAccelFsG = 16.0f;    // ±16 g  (headroom for impact spike)
  static constexpr float kGyroFsDps = 500.0f;  // ±500 °/s

  explicit Mpu6050(TwoWire& wire = Wire, uint8_t address = 0x68)
      : wire_(wire), address_(address) {}

  // Wakes the device, verifies WHO_AM_I, and applies the configuration above.
  // Returns false if the device does not respond as an MPU-6050.
  bool begin();

  // Reads one sample. Returns false on an I2C error.
  bool read(Sample& out);

private:
  bool writeReg(uint8_t reg, uint8_t value);
  bool readRegs(uint8_t reg, uint8_t* buf, size_t len);

  TwoWire& wire_;
  uint8_t address_;
};
