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
Connect the display to the Pi's GPIO header using the following pinout:

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
| **SDO** (MISO) | SPI0 MISO | Pin 21 (GPIO 9) - *Optional* |

### 2. Enable SPI
1. Run `sudo raspi-config`.
2. Navigate to **Interface Options** > **SPI**.
3. Select **Yes** to enable.
4. Finish and reboot.

### 3. Software Configuration (fbtft)
This enables the built-in Linux kernel driver for SPI displays.
1. Open the boot config file:
   - *Newer OS (Bookworm):* `sudo nano /boot/firmware/config.txt`
   - *Older OS (Bullseye):* `sudo nano /boot/config.txt`
2. Add the following line to the bottom:
   ```bash
   dtoverlay=fbtft,spi0-0,ili9341,rotate=270,speed=32000000,fps=30,dc_pin=24,reset_pin=25
   ```
   *(Note: Adjust `rotate` to 90, 180, or 270 to match your physical orientation).*
3. Save and Reboot. The screen should now initialize and be accessible via `/dev/fb1`.

---

## Quick Start

### 1. Install Build Tools
**Debian/Ubuntu/Raspberry Pi OS:**
```bash
sudo apt-get update
sudo apt-get install musl-tools make gcc
```

### 2. Build
```bash
git clone <repository>
cd m8alt
make clean && make
```

### 3. Configure
Edit `config.ini` to match your hardware. If using the SPI display configured above, change the framebuffer device:
```ini
[system]
serial_device=/dev/ttyACM0      # M8 USB serial port
framebuffer_device=/dev/fb1     # Use /dev/fb1 for SPI displays
input_device=/dev/input/event4  # Keyboard/gamepad
```

### 4. Run
```bash
sudo ./m8alt
```

---

## Compilation & Makefile Flags

Building a static binary with `musl-gcc` on a system that primarily uses `glibc` requires specific compiler flags to find Linux kernel headers without causing library conflicts.

### Key Flags Explained
- `-static`: Forces the linker to include all library code within the binary.
- `-Isrc`: Adds the project's source directory to the header search path.
- `-idirafter /usr/include`: Crucial for finding `linux/fb.h` and `linux/input.h` without mixing glibc headers.
- `-idirafter /usr/include/$(shell gcc -dumpmachine)`: Points to architecture-specific headers (like `asm/ioctl.h`).

---

## Configuration Reference

### System Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `serial_device` | `/dev/ttyACM0` | M8 USB serial port. |
| `framebuffer_device` | `/dev/fb0` | `/dev/fb0` = HDMI, `/dev/fb1` = SPI/Small LCD. |
| `input_device` | `/dev/input/event0` | Input event device for keyboard/controller. |

### Keyboard Mapping
Default mapping uses standard Linux input event codes:

| M8 Function | Key | Event Code |
|-------------|-----|------------|
| **Up** | Arrow Up | 103 |
| **Down** | Arrow Down | 108 |
| **Left** | Arrow Left | 105 |
| **Right** | Arrow Right | 106 |
| **Select** | Left Shift | 42 |
| **Start** | Space | 57 |
| **Opt** | Left Ctrl | 29 |
| **Edit** | Left Alt | 56 |

---

## Architecture & Performance

### Rendering Pipeline
- **Native Format Rendering**: Detects depth (RGB565/ARGB8888) and packs colors once.
- **Dirty Rectangle Optimization**: Only modified regions are copied to the framebuffer.
- **VSync Prevention**: Uses `FBIO_WAITFORVSYNC` to eliminate tearing.

### Input Processing
- Single-threaded with `poll()` multiplexing.
- Sub-millisecond latency from keypress to M8.

---

## Troubleshooting

### M8 Not Connecting
```bash
dmesg | grep tty
# Should show: cdc_acm 1-1:1.0: ttyACM0: USB ACM device
```

### Display Issues
**Black Screen:**
```bash
# Check if framebuffer is accessible
cat /dev/urandom > /dev/fb1
# Should show noise on the SPI screen
```

**Wrong Colors:**
- Supports RGB565 (16-bit) and ARGB8888 (32-bit).
- Check `fbset -i` output for `bits_per_pixel`.

---

## Performance Benchmarks
**Raspberry Pi Zero 2W:**
- Frame Time: ~2-4ms.
- Input Latency: <1ms.
- CPU Usage: ~15-20% (single core).

---

## License
This project incorporates:
- **SLIP decoder**: MIT (Marcin Borowicz).
- **INI parser**: MIT (rxi).
- **Original m8c**: MIT.

---

## Known Limitations
- **Single M8 Support**: Only one M8 connection at a time.
- **No Audio Routing**: Display/input only.
- **Fixed Resolution**: 320×240 native.
