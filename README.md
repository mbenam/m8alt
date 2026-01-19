# M8 Headless Client (Embedded)

A lightweight, bare-metal C implementation of the M8 headless display client optimized for **Embedded Linux** devices (Raspberry Pi Zero 2W, Luckfox Pico, etc.). This version replaces SDL dependencies with direct framebuffer rendering for maximum performance on low-power ARM processors.

## Overview

**m8alt** is a stripped-down port of the m8c client designed specifically for resource-constrained embedded systems. It provides:

- **Zero Dependencies**: No SDL, libusb, X11, or Wayland required.
- **Direct Framebuffer Access**: Renders to `/dev/fb0` with hardware-accelerated vsync.
- **Static Binary**: Compiled with `musl-libc` for portability across embedded Linux systems.
- **High Performance**: Optimized for single-core ARMv7 processors.
- **Low Latency**: Dirty rectangle tracking minimizes bus traffic.
- **Evdev Input**: Direct `/dev/input/eventX` reading for minimal overhead.

---

## Hardware Requirements

### Minimum Specifications
- **CPU**: ARMv7 or newer (tested on BCM2710 - Pi Zero 2W).
- **RAM**: 32MB+.
- **Display**: Framebuffer-compatible display (HDMI, SPI LCD, etc.).
- **Serial**: USB connection to Dirtywave M8 Tracker.

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

Edit `config.ini` to match your hardware:

```ini
[system]
serial_device=/dev/ttyACM0      # M8 USB serial port
framebuffer_device=/dev/fb0     # HDMI or SPI display
input_device=/dev/input/event4  # Keyboard/gamepad
```

### 4. Run

```bash
sudo ./m8alt
```

---

## Compilation & Makefile Flags

Building a static binary with `musl-gcc` on a system that primarily uses `glibc` (like Raspberry Pi OS) requires specific compiler flags to find Linux kernel headers without causing library conflicts.

### Key Flags Explained

- `-static`: Forces the linker to include all library code within the binary. This ensures the resulting `m8alt` file can run on any Linux system (even those without musl installed).
- `-Isrc`: Adds the project's source directory to the header search path.
- `-idirafter /usr/include`: **Crucial Flag.** `musl-gcc` intentionally ignores the standard `/usr/include` directory to prevent mixing incompatible glibc headers. However, we need Linux-specific headers (`linux/fb.h` and `linux/input.h`). This flag tells the compiler to search the system path *only as a last resort* if it cannot find the header in the musl environment.
- `-idirafter /usr/include/$(shell gcc -dumpmachine)`: Points the compiler to architecture-specific headers (like `asm/ioctl.h`) required by the Linux framebuffer and input subsystems.

### Troubleshooting Build Errors
If you see an error like `fatal error: bits/libc-header-start.h`, it means the compiler is accidentally trying to use glibc headers. Ensure you are using `-idirafter` instead of `-isystem` or `-I` for the `/usr/include` directories.

---

## Configuration Reference

### System Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `serial_device` | `/dev/ttyACM0` | M8 USB serial port (check `dmesg` after plugging in M8). |
| `framebuffer_device` | `/dev/fb0` | Display device (`/dev/fb0` = HDMI, `/dev/fb1` often = SPI). |
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

**Find Event Codes:**
```bash
sudo evtest /dev/input/event4
# Press keys to see their codes
```

---

## Architecture & Performance

### Rendering Pipeline

**Native Format Rendering:**
- Detects framebuffer depth at startup (16-bit RGB565 or 32-bit ARGB8888).
- Allocates internal buffer matching native pixel format.
- Colors are packed **once** during draw operations.
- Blit phase is a raw `memcpy` with zero per-pixel conversion overhead.

**Dirty Rectangle Optimization:**
- Tracks modified regions per frame (`min_x`, `min_y`, `max_x`, `max_y`).
- Only modified regions are copied to framebuffer.
- Full-screen updates only when necessary (screen transitions).

**VSync Prevention:**
- Uses `FBIO_WAITFORVSYNC` ioctl to synchronize with display refresh.
- Eliminates horizontal tearing on waveform updates.

### Input Processing

**Event Loop:**
- Single-threaded with `poll()` multiplexing.
- Non-blocking reads on serial and input file descriptors.
- Sub-millisecond latency from keypress to M8.

---

## Font System

Five embedded bitmap fonts are selected automatically based on M8's `CMD_SYSTEM_INFO` response to ensure correct rendering for both M8 v1 and v2 hardware/firmware versions.

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
cat /dev/urandom > /dev/fb0
# Should show noise on the screen
```

**Wrong Colors:**
- Supports RGB565 (16-bit) and ARGB8888 (32-bit).
- Automatically detects format via `ioctl`.
- Check `fbset -i` output for `bits_per_pixel`.

---

## Performance Benchmarks

**Raspberry Pi Zero 2W (1GHz Quad-Core ARM Cortex-A53):**
- Frame Time: ~2-4ms.
- Input Latency: <1ms (evdev to M8).
- CPU Usage: ~15-20% (single core).
- Memory: ~8MB RSS.

---

## License

This project incorporates:
- **SLIP decoder**: MIT License (Marcin Borowicz).
- **INI parser**: MIT License (rxi).
- **Original m8c**: MIT License.

---

## Known Limitations

- **Single M8 Support**: Only one M8 connection at a time.
- **No Audio Routing**: This is a display/input client only.
- **Fixed Resolution**: 320×240 native, no scaling.
- **Linux Only**: Requires Linux framebuffer and evdev subsystems.
