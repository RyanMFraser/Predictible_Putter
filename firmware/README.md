# Firmware (ESP32-WROOM-32, PlatformIO + Arduino)

Slice 1: streams raw 1 kHz MPU-6050 samples over USB serial as CSV
(`t_us,ax,ay,az,gx,gy,gz`, raw int16 counts per Spec 0001 §4.4). Used to prove the
sensor works and to capture a real putt impact for setting ranges/thresholds.

## Wiring (MPU-6050 → ESP32 DevKit)
| MPU-6050 | ESP32     | Notes |
|----------|-----------|-------|
| VCC      | 3V3       | module has an onboard regulator |
| GND      | GND       |       |
| SDA      | GPIO 21   | I2C data (default) |
| SCL      | GPIO 22   | I2C clock (default) |
| AD0      | GND       | sets I2C address to 0x68 |
| INT      | —         | unused in Slice 1 |

## Install PlatformIO (once)
```bash
pip3 install --user platformio      # provides the `pio` CLI
pio --version                        # confirm it's on PATH
```

## Expose the board to WSL (usbipd-win)
The CP2102 shows up as a USB device on **Windows**; WSL doesn't see it until you forward it
in with usbipd-win. All `usbipd` commands run in an **Administrator PowerShell on Windows**,
not inside WSL.

**Install usbipd (once, on Windows):**
```powershell
winget install usbipd
```
If `winget` isn't available, grab the `.msi` from
https://github.com/dorssel/usbipd-win/releases/latest and run it.
**Then close and reopen the Administrator PowerShell** so `usbipd` is on PATH — an already-open
window won't pick it up (symptom: `usbipd : The term 'usbipd' is not recognized`).

**Forward the board (each session):**
```powershell
usbipd list                          # find the "Silicon Labs CP210x" bus id, e.g. 2-3
usbipd bind --busid 2-3              # once per device (needs admin)
usbipd attach --wsl --busid 2-3      # each time you plug in / reboot WSL
```
Requires WSL 2 (`wsl --version` to check). Back in WSL the board appears as `/dev/ttyUSB0`:
```bash
ls -l /dev/ttyUSB0                    # add yourself to `dialout` or use sudo if permission denied
```

## Build, flash, monitor
```bash
cd firmware
pio run                                        # build
pio run -t upload --upload-port /dev/ttyUSB0   # flash
pio device monitor -b 921600 -p /dev/ttyUSB0   # watch serial (Ctrl+C to quit)
```
You should see the `# I2C scan:` line (`device at 0x68` = sensor found), then the
`# putter-slice1` header, then CSV rows. Move the sensor to confirm the numbers respond.

**Gotchas we hit during bring-up:**
- *Upload fails with "port is busy / Resource temporarily unavailable"* — a serial monitor
  still has `/dev/ttyUSB0` open. Close it (Ctrl+C) before uploading; only one program can
  hold the port.
- *Monitor opened mid-stream, missed the header* — the `# ...` header only prints once at
  boot. Press the **EN** (reset) button once to reboot and re-print it.
- *"MPU-6050 not found" loop* — I²C wiring. Check the `# I2C scan:` output: `0x69` instead
  of `0x68` means AD0 is HIGH (tie it to GND); nothing found means swapped SDA/SCL, a loose
  jumper, or no common ground. See `../hardware/README.md`.

### Capture a putt to a file (for offline analysis)
`pio device monitor` needs an interactive terminal. To log the raw stream to a CSV file
instead, read the port directly:
```bash
# stop any monitor first, then:
stty -F /dev/ttyUSB0 921600 raw -echo
cat /dev/ttyUSB0 > putt-capture.csv           # Ctrl+C when done
```
(Recorded `*.csv` files are git-ignored by default — see the repo `.gitignore`.)
