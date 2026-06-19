# CardMeteo_AHT_BMP

Weather station for M5Stack Cardputer ADV with AHT20 + BMP280 sensors.

## Status

Old personal project shared for others. Refactored for readability, removed some incomplete features. Works but not actively maintained. Fork and modify if you find it useful.

## Wiring

Connect your sensor board to the Cardputer ADV Grove port:

```
─────────────────────────────────────────
Sensor Board     |    Cardputer ADV Grove
─────────────────────────────────────────
SCL  ───────────────────────────────> G1
GND  ───────────────────────────────> G
SDA  ───────────────────────────────> G2
VDD  ───────────────────────────────> 5V
```

## Controls

| Key | Action |
|-----|--------|
| **Space / Enter** | Dashboard ↔ Forecast ↔ Stats (context back) |
| **T** | Toggle Stats window |
| **P** | Toggle pressure box on dashboard |
| **L** | Toggle optional SD card CSV logging |
| **M** | Open/close settings (save & exit on close) |
| **E / S** (or `;` / `.`) | Navigate settings menu (up / down) |
| **A / D** (or `,` / `/`) | Decrease / increase value |
| **1–5** | Switch theme (Default, Hacker, Ocean, Sunset, Light) |
| **0** | Toggle instant dim (absolute black) |
| **Esc / \`** | Restart (exit to M5Launcher) |

## Features

- **Dashboard**: Temperature, humidity, local pressure (toggle with P)
- **Forecast**: Sea-level pressure, weather prediction with adaptive trend detection
  - Pressure trend uses 5-min intervals with adaptive thresholds (1.5 hPa → 0.5 hPa)
  - 3-hour pressure history ring buffer with least-squares rate (hPa/h)
  - Zambretti forecaster (pressure + 3h tendency) shown in Stats
  - Confidence: collecting → potential → confirmed
- **Stats (T)**: Rate of change, Zambretti code, min/max/avg pressure, trend, live CPU MHz, battery
- **Settings**: Altitude, update interval, sound, brightness, auto-dim timeout, CPU frequency
- **Auto-dim**: Screen blanks after inactivity; in Auto CPU underclocks to 80 MHz while dimmed (skips when in settings)
- **SD logging (L)**: Optional CSV log to `/meteo.csv`, one row every 10 min. Off by default, no SD card needed
- **Battery friendly**: BMP280 runs in FORCED mode (sleeps between reads); longer loop delay while dimmed
- **5 themes**: Color schemes with header/box/text colors

## On Device

<div align="left">
  <img src="./media/image.jpg" width="360" alt="CardMeteo Dashboard">
</div>


## Build & Flash

### Option 1: Direct Flash (Fastest)
1. Download the latest firmware from the [Releases](../../releases) page.
2. Flash it using standard tools like **esptool.py** or copy it directly to your SD card to use with **M5Launcher**.

### Option 2: Via VS Code
1. Install **VS Code** and the **PlatformIO IDE** extension.
2. Open this project folder in VS Code.
3. Connect your Cardputer via USB-C.
4. Click the **PlatformIO: Upload** button (the arrow icon in the bottom status bar) to build and flash the device.

### Option 3: Via Command Line (CLI)
Make sure you have `platformio` installed in your terminal, then run:

```bash
# Install dependencies and build the firmware
platformio run

# Build, flash to device, and open the serial monitor
platformio run --target upload --target monitorr