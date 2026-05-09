# HoloDisplay — ESP32-C3 Round Clock

A minimal, clean firmware for the **Elecrow ESP32-2424S012N** (1.28" round GC9A01 display, ESP32-C3) that shows a live clock face and receives messages and time sync over Bluetooth LE — designed to work with **Android Tasker**.

## Features

- **Animated clock face** — second sweep arc (cyan → orange), hour tick marks, bezel ring
- **BLE NUS** (Nordic UART Service) — compatible with Tasker, nRF Connect, and any NUS client
- **Time sync** via BLE — no RTC chip required
- **Message display** — text slides up from the bottom of the screen
- **BLE heartbeat indicator** — pulses green when connected
- **No-flicker rendering** — full frame rendered in an off-screen sprite before pushing
- **Clean boot** — backlight off during flush, no ghost pixels from previous firmware

## Hardware

| Board | Elecrow ESP32-2424S012N |
|-------|------------------------|
| MCU   | ESP32-C3 @ 160 MHz     |
| Display | GC9A01 240×240 round, SPI |
| Flash | 4 MB                   |

### Wiring (built-in on this board)

| Signal | GPIO |
|--------|------|
| SCLK   | 6    |
| MOSI   | 7    |
| DC     | 2    |
| CS     | 10   |
| BL     | 3    |

## Getting Started

### Requirements

- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)

### Build & Flash

```bash
pio run -e holographic_esp32_display --target upload
```

### Monitor Serial

```bash
pio device monitor
```

## BLE Protocol

The device advertises as **`HoloDisplay`** using the Nordic UART Service (NUS).

| Characteristic | UUID |
|----------------|------|
| Service | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX (write here) | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| TX (notify) | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |

### Commands (write to RX characteristic)

| Command | Example | Effect |
|---------|---------|--------|
| `TIME:<epoch>` | `TIME:1746835200` | Sync the clock |
| `MSG:<text>` | `MSG:Hello!` | Show a message |
| `<bare text>` | `Meeting now` | Same as MSG |

## Tasker Setup

1. Install the **BLE Scanner** or use Tasker's built-in BLE Write action
2. Connect to `HoloDisplay`
3. Add a task with **BLE Write** → service `6E400001...`, characteristic `6E400002...`
4. To sync time: write `TIME:%TIMES` (Tasker's Unix epoch variable)
5. To push a notification: write `MSG:%NTITLE`

## Adding a GIF

LovyanGFX has built-in GIF decoding. To embed an animation:

```bash
# 1. Convert GIF to a C byte array
xxd -i animation.gif | sed 's/unsigned/const/' > src/anim.h
```

```cpp
// 2. Include it in display.cpp
#include "anim.h"

// 3. Draw it after canvas.pushSprite()
tft.drawGif(animation_gif, animation_gif_len, x, y);
```

Keep GIFs small (e.g. 80×80 px) — the ESP32-C3 has no PSRAM.

## Project Structure

```
src/
  main.cpp       — setup / loop
  display.h/.cpp — LGFX config, clock face rendering
  ble_nus.h/.cpp — BLE NUS server, TIME/MSG command handling
boards/
  esp32-2424S012N.json — custom board definition
partitions.csv         — OTA-ready partition table
platformio.ini
```

## License

MIT
