# M8 Headless Client (Embedded)

A lightweight, bare-metal C implementation of the M8 headless display client optimized for **Embedded Linux** devices (Raspberry Pi Zero 2W, Luckfox Pico, etc.). This version replaces SDL dependencies with direct framebuffer rendering for maximum performance on low-power ARM processors.

## Overview

**m8alt** is a stripped-down port of the m8c client designed specifically for resource-constrained embedded systems. It provides:

- **Zero Dependencies**: No SDL, libusb, X11, or Wayland required.
- **Direct Framebuffer Access**: Renders to `/dev/fb0` or `/dev/fb1` with hardware-accelerated vsync.
- **Static Binary**: Compiled with `musl-libc` for portability across embedded Linux systems.
- **High Performance**: Optimized for single-core ARMv7 processors.
- **Low Latency**: Dirty rectangle tracking minimizes bus traffic.
- **Evdev Input**: Direct `/dev/input/eventX` reading for minimal overhead.

---

## Hardware Requirements

### Minimum Specifications
- **CPU**: ARMv7 or newer (tested on BCM2710 - Pi Zero 2W, BCM2837 - Pi 3B+).
- **RAM**: 32MB+.
- **Display**: Framebuffer-compatible display (HDMI, SPI LCD, etc.).
- **Serial**: USB connection to Dirtywave M8 Tracker.

---

## SPI Display Setup (ILI9341)

If you are using a 2.4" or 2.8" ILI9341 SPI display (common for handheld builds) on a Raspberry Pi, follow these steps.

### 1. Wiring Diagram

| ILI9341 Pin | RPi Pin Name | RPi Physical Pin |
| :--- | :--- | :--- |
| **VCC** | 3.3V | Pin 1 or 17 |
| **GND** | Ground | Pin 6, 9, 14, 20, or 25 |
| **CS** (Chip Select) | SPI0 CE0 | Pin 24 (GPIO 8) |
| **RESET** | GPIO 25 | Pin 22 |
| **DC** (Data/Command) | GPIO 24 | Pin 18 |
| **SDI** (MOSI) | SPI0 MOSI | Pin 19 (GPIO 10) |
| **SCK** (Clock) | SPI0 SCLK | Pin 23 (GPIO 11) |
| **LED** (Backlight) | 3.3V or GPIO 18 | Pin 1 or Pin 12 (PWM) |

### 2. Enable SPI & Driver
1. Enable SPI via `sudo raspi-config` (**Interface Options** > **SPI**).
2. Add the following line to `/boot/firmware/config.txt` (or `/boot/config.txt`):
   ```bash
   dtoverlay=fbtft,spi0-0,ili9341,rotate=270,speed=32000000,fps=30,dc_pin=24,reset_pin=25
   ```
3. Reboot. Your display should now be available at `/dev/fb1`.

---

## Architecture & Modules

The application is single-threaded for deterministic performance. The main loop uses `poll()` to multiplex serial data from the M8 and input events from `evdev`, while the display subsystem manages its own state and optimization logic.

### A. Display (src/display.c, src/display.h)

#### 1. Rendering Pipeline (Native Buffer)
To eliminate conversion overhead during the critical flush phase, the renderer detects the framebuffer depth at startup:
- **Initialization**: Detects `bits_per_pixel` (16 or 32) via `ioctl`.
- **Buffer Allocation**: Allocates an internal RAM buffer (`render_buffer`) matching the native screen format (**RGB565** or **ARGB8888**).
- **Draw Time**: Colors are packed into the native format **once** when drawing primitives.
- **Blit Time**: The flush operation is a raw `memcpy` (or optimized block copy), requiring zero math per pixel.

#### 2. Dirty Rectangle Tracking
Instead of redrawing the full 320x240 screen every frame:
- The system tracks `min_x`, `min_y`, `max_x`, and `max_y` of any pixel modified during a frame.
- **Aggressive Padding**: A safety margin (2px horizontal, 6px vertical) is added to dirty regions to ensure font "tails" and vertical offsets are correctly cleared, preventing artifacts.
- **Optimization**: If no pixels change, `display_blit` returns immediately.

#### 3. Artifact Prevention & Logic
- **Font Offsets**: Text is drawn with a vertical offset defined in the font headers.
- **Absolute vs. Relative Clears**: 
    - **Text/Cursors**: Drawn relative to the font offset.
    - **Clears (Black/Background)**: Detected by color (0x00 or global background). These are treated as **absolute coordinates** to ensure the entire line/screen is wiped clean, fixing "ghosting" artifacts during screen transitions.
- **Space Character (ASCII 32)**: Explicitly draws a background-colored rectangle to erase underlying text.
- **VSync**: Uses `ioctl(fb_fd, FBIO_WAITFORVSYNC, ...)` to synchronize with the display refresh rate, preventing horizontal tearing on rapid waveform updates.

#### 4. Drawing Primitives
- **display_draw_char**: Features inlined bitmap parsing and direct bit-shift extraction. It writes directly to native pointers using pointer arithmetic rather than array indexing.
- **display_draw_rect**: Uses `memset` for black/clear operations for maximum speed and pointer increment loops for colored rectangles.
- **display_draw_waveform**: Implements **Bresenham's line algorithm**. It clears only the specific column/area used by the previous frame's waveform before drawing the new one.

---

## Quick Start

### 1. Build
**Debian/Ubuntu/Raspberry Pi OS:**
```bash
sudo apt-get install musl-tools make gcc
git clone <repository>
cd m8alt
make
```

### 2. Configure
Edit `config.ini`:
```ini
[system]
serial_device=/dev/ttyACM0      # M8 USB
framebuffer_device=/dev/fb1     # Use fb1 for SPI, fb0 for HDMI
input_device=/dev/input/event4  # Check via 'evtest'
```

### 3. Run
```bash
sudo ./m8alt
```

---

## Compilation & Makefile Flags

Building a static binary with `musl-gcc` on glibc-based systems requires:
- `-static`: Includes all library code within the binary.
- `-idirafter /usr/include`: Allows access to `linux/fb.h` and `linux/input.h` without causing glibc header conflicts.

---

## Configuration Reference

### Keyboard Mapping (Default)

| M8 Function | Key | Event Code |
|-------------|-----|------------|
| **Up / Down / Left / Right** | Arrow Keys | 103, 108, 105, 106 |
| **Select** | Left Shift | 42 |
| **Start** | Space | 57 |
| **Opt** | Left Ctrl | 29 |
| **Edit** | Left Alt | 56 |

---

## Troubleshooting

### Display Issues
**Black Screen:**
```bash
# Test the framebuffer directly
cat /dev/urandom > /dev/fb1
```
**Wrong Colors:**
- Supports RGB565 (16-bit) and ARGB8888 (32-bit). Check `fbset -i`.

---

## License
- **SLIP decoder**: MIT (Marcin Borowicz).
- **INI parser**: MIT (rxi).
- **Original m8c**: MIT.

---

## Known Limitations
- **No Audio Routing**: This is a display/input client only.
- **Fixed Resolution**: 320×240 native.
