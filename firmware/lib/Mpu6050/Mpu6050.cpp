#include "Mpu6050.h"

namespace {
// MPU-6050 register map (subset we use).
constexpr uint8_t REG_SMPLRT_DIV = 0x19;
constexpr uint8_t REG_CONFIG = 0x1A;        // DLPF config
constexpr uint8_t REG_GYRO_CONFIG = 0x1B;   // FS_SEL
constexpr uint8_t REG_ACCEL_CONFIG = 0x1C;  // AFS_SEL
constexpr uint8_t REG_ACCEL_XOUT_H = 0x3B;  // start of accel/temp/gyro block
constexpr uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr uint8_t REG_WHO_AM_I = 0x75;

constexpr uint8_t WHO_AM_I_VALUE = 0x68;

// Config values (Spec 0001 §4.1). DLPF_CFG=0 -> widest bandwidth (accel 260 Hz),
// so the short impact transient is filtered as little as the sensor allows.
constexpr uint8_t DLPF_CFG_WIDEST = 0x00;
constexpr uint8_t ACCEL_FS_16G = 0x18;   // AFS_SEL = 3
constexpr uint8_t GYRO_FS_500DPS = 0x08; // FS_SEL = 1
constexpr uint8_t CLKSEL_PLL_GYRO_X = 0x01;
}  // namespace

bool Mpu6050::begin() {
  uint8_t who = 0;
  if (!readRegs(REG_WHO_AM_I, &who, 1) || who != WHO_AM_I_VALUE) {
    return false;
  }
  // Wake up and use the gyro X PLL as clock source (more stable than internal osc).
  if (!writeReg(REG_PWR_MGMT_1, CLKSEL_PLL_GYRO_X)) return false;
  delay(10);
  if (!writeReg(REG_CONFIG, DLPF_CFG_WIDEST)) return false;
  if (!writeReg(REG_GYRO_CONFIG, GYRO_FS_500DPS)) return false;
  if (!writeReg(REG_ACCEL_CONFIG, ACCEL_FS_16G)) return false;
  // We pace sampling ourselves in the main loop, so SMPLRT_DIV is not critical;
  // set it to 0 (no extra division) for completeness.
  if (!writeReg(REG_SMPLRT_DIV, 0x00)) return false;
  return true;
}

bool Mpu6050::read(Sample& out) {
  // Read the 14-byte block: accel(6) + temp(2) + gyro(6). Temp is discarded.
  uint8_t buf[14];
  if (!readRegs(REG_ACCEL_XOUT_H, buf, sizeof(buf))) return false;

  auto be16 = [](uint8_t hi, uint8_t lo) -> int16_t {
    return static_cast<int16_t>((static_cast<uint16_t>(hi) << 8) | lo);
  };
  out.ax = be16(buf[0], buf[1]);
  out.ay = be16(buf[2], buf[3]);
  out.az = be16(buf[4], buf[5]);
  // buf[6..7] = temperature (skipped)
  out.gx = be16(buf[8], buf[9]);
  out.gy = be16(buf[10], buf[11]);
  out.gz = be16(buf[12], buf[13]);
  return true;
}

bool Mpu6050::writeReg(uint8_t reg, uint8_t value) {
  wire_.beginTransmission(address_);
  wire_.write(reg);
  wire_.write(value);
  return wire_.endTransmission() == 0;
}

bool Mpu6050::readRegs(uint8_t reg, uint8_t* buf, size_t len) {
  wire_.beginTransmission(address_);
  wire_.write(reg);
  if (wire_.endTransmission(false) != 0) return false;  // repeated start
  const size_t got = wire_.requestFrom(address_, static_cast<uint8_t>(len));
  if (got != len) return false;
  for (size_t i = 0; i < len; ++i) buf[i] = wire_.read();
  return true;
}
