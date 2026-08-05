# Hardware — Predictible Putter

Physical build notes for the device: components, wiring, and assembly. This is the
source of truth for **how the hardware is connected**. Firmware pin assignments
(`firmware/src/main.cpp`) and the wiring here must stay in sync.

See `CLAUDE.md` §3 for the higher-level hardware facts and rationale.

## Components

| Part | Details |
|------|---------|
| MCU / board | ESP32-WROOM-32 on a generic DevKit (DOIT DevKit V1 / ESP32 DevKitC family). Has BLE. |
| USB-serial | CP2102 (Silicon Labs CP210x). Flash/power over the Micro-USB port (data-capable cable). |
| Sensor | MPU-6050 6-axis IMU (3-axis accel + 3-axis gyro), I²C breakout. |
| Mounting | Rear cavity/indent of a mallet-style putter head (behind the face). |

## Wiring: MPU-6050 → ESP32 DevKit

I²C on the ESP32's default pins, sensor at address `0x68`.

| MPU-6050 pin | ESP32 pin | Purpose | Required? |
|--------------|-----------|---------|-----------|
| VCC | 3V3 | Power (3.3 V — **do not** use 5V/VIN) | Yes |
| GND | GND | Ground (common with ESP32) | Yes |
| SDA | GPIO21 (D21) | I²C data | Yes |
| SCL | GPIO22 (D22) | I²C clock | Yes |
| AD0 | GND | Sets I²C address to `0x68` | Yes (see note) |
| INT | — | Interrupt, unused in Slice 1 | No — leave open |
| XDA / XCL / ASDA / ASCL | — | Auxiliary I²C (external magnetometer) | No — leave open |

```
   ESP32 DevKit                 MPU-6050
  ┌───────────┐               ┌──────────┐
  │      3V3  ├──────────────►│ VCC      │
  │      GND  ├──────┬───────►│ GND      │
  │    GPIO21 ├──────┼───────►│ SDA      │
  │    GPIO22 ├──────┼───────►│ SCL      │
  │           │      └───────►│ AD0      │  (AD0 tied to GND ⇒ addr 0x68)
  └───────────┘               │ INT  ─── │  (unused)
                              └──────────┘
```

### Notes / gotchas
- **Power from 3V3, not 5V.** The breakout's onboard regulator and logic run at 3.3 V;
  the ESP32 3V3 pin is the safe source. Do not feed VIN/5V.
- **Common ground.** ESP32 GND and MPU-6050 GND must share the same rail/net.
- **AD0 → GND is what selects address `0x68`.** The firmware talks to `0x68`
  (`firmware/lib/Mpu6050/Mpu6050.h`). If AD0 floats the address can be unstable; if it's
  tied HIGH (3V3) the address becomes `0x69` and `imu.begin()` fails with the
  `# ERROR: MPU-6050 not found` loop on the serial monitor. Some breakouts pull AD0 low
  internally, but wiring it explicitly removes all doubt.
- **SDA/SCL not swapped.** Swapping them is the other common cause of a "not found" error.

## Bring-up checklist

1. Wire per the table above; double-check AD0 → GND and SDA/SCL orientation.
2. Connect the ESP32 via a **data-capable** Micro-USB cable.
3. Expose the board to WSL and flash — see `firmware/README.md` (usbipd + `pio run -t upload`).
4. Open the serial monitor at **921600** baud.
5. Expect the `# putter-slice1 v1` header, then CSV rows (`t_us,ax,ay,az,gx,gy,gz`) that
   change as you move the sensor. The `# ERROR: MPU-6050 not found` loop means wiring
   (usually AD0 or swapped SDA/SCL).

## Mounting (later)

The sensor mounts rigidly in the rear cavity of a mallet putter head, behind the face.
Exact axis orientation relative to the target line / vertical / face-normal is **TBD** and
will be fixed and calibrated once physically mounted (CLAUDE.md §3, Spec 0002 §4.1).
Record the final orientation and calibration procedure here once decided.
