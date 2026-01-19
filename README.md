# M8 Headless Client (Embedded)

A lightweight, bare-metal C implementation of the M8 headless display client optimized for **Embedded Linux** devices (Raspberry Pi Zero 2W, Luckfox Pico, etc.). This version replaces SDL dependencies with direct framebuffer rendering for maximum performance on low-power ARM processors.

## Overview

**m8alt** is a stripped-down port of the m8c client designed specifically for resource-constrained embedded systems. It provides:

- **Zero Dependencies**: No SDL, libusb, X11, or Wayland required
- **Direct Framebuffer Access**: Renders to `/dev/fb0` with hardware-accelerated vsync
- **Static Binary**: Compiled with musl-libc for portability across embedded Linux systems
- **High Performance**: Optimized for single-core ARMv7 processors (Pi Zero 2W tested)
- **Low Latency**: Dirty rectangle tracking minimizes bus traffic
- **Evdev Input**: Direct `/dev/input/eventX` reading for minimal overhead
- **Autoplay Support**: Automated key sequences for navigation and startup routines

---

## Hardware Requirements

### Minimum Specifications
- **CPU**: ARMv7 or newer (tested on BCM2710 - Pi Zero 2W)
- **RAM**: 32MB+ 
- **Display**: Framebuffer-compatible display (HDMI, SPI LCD, etc.)
- **Serial**: USB connection to Dirtywave M8 Tracker

### Tested Platforms
- Raspberry Pi Zero 2W
- Raspberry Pi 3/4
- Luckfox Pico (RV1106)
- Generic Embedded Linux (Buildroot, Yocto)

---

## Quick Start

### 1. Install Build Tools

**Debian/Ubuntu/Raspberry Pi OS:**
```bash
sudo apt-get update
sudo apt-get install musl-tools make gcc
```

**Manual musl Installation:**
```bash
wget https://musl.libc.org/releases/musl-1.2.4.tar.gz
tar -xzf musl-1.2.4.tar.gz
cd musl-1.2.4
./configure --prefix=/usr/local/musl
make && sudo make install
export PATH=/usr/local/musl/bin:$PATH
```

### 2. Build

```bash
git clone <repository>
cd m8alt
make clean && make
```

This produces a **static binary** (`m8alt`) that can be copied to any embedded Linux system without dependencies.

### 3. Configure

Edit `config.ini` to match your hardware:

```ini
[system]
serial_device=/dev/ttyACM0      # M8 USB serial port
framebuffer_device=/dev/fb0     # HDMI or SPI display
input_device=/dev/input/event4  # Keyboard/gamepad
```

**Finding Your Input Device:**
```bash
ls -l /dev/input/by-id/
# Example output:
# usb-Logitech_USB_Keyboard-event-kbd -> ../event4
```

### 4. Run

```bash
sudo ./m8alt
```

**Permissions Note:** Requires read/write access to:
- `/dev/fb0` (framebuffer)
- `/dev/ttyACM0` (serial)
- `/dev/input/eventX` (keyboard)

**Run without sudo:**
```bash
sudo usermod -a -G video,dialout,input $USER
# Log out and back in, then:
./m8alt
```

---

## Configuration Reference

### System Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `serial_device` | `/dev/ttyACM0` | M8 USB serial port (check `dmesg` after plugging in M8) |
| `framebuffer_device` | `/dev/fb0` | Display device (`/dev/fb0` = HDMI, `/dev/fb1` often = SPI) |
| `input_device` | `/dev/input/event0` | Input event device for keyboard/controller |

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

**Custom Mapping Example:**
```ini
[keyboard]
key_up=103
key_down=108
key_left=105
key_right=106
key_select=30    # A key
key_start=31     # S key
key_opt=44       # Z key
key_edit=45      # X key
```

### Autoplay Configuration

Autoplay executes predefined key sequences automatically after M8 connection. Useful for:
- Auto-starting playback on boot
- Navigating to a specific screen
- Loading a project automatically

```ini
[autoplay]
enabled=1
start_delay=2000        # Wait 2 seconds after M8 connects
command_delay=100       # Base delay between all commands (ms)
sequence=start:500,down:200,select+up:300
```

#### Autoplay Syntax

**Single Keys:**
```ini
sequence=start:500,down:200,right:100
```

**Combo Keys** (using `+`):
```ini
sequence=select+up:500,opt+down:300,select+start:400
```

**Complex Example:**
```ini
; Navigate to Song screen, move down 3 times, enter edit mode
sequence=select+up:1000,down:200,down:200,down:200,edit:500
```

#### Timing Behavior

Total delay = `command_delay` + command-specific delay

Example:
```ini
command_delay=100
sequence=start:500,down:200
```

Timeline:
1. Press **START** → hold 50ms → release
2. Wait **600ms** (100 + 500)
3. Press **DOWN** → hold 50ms → release
4. Wait **300ms** (100 + 200)

#### Common M8 Navigation Combos

| Action | Command | Description |
|--------|---------|-------------|
| **Project Screen** | `select+up` | Jump to project/song screen |
| **Mixer** | `select+down` | Jump to mixer |
| **Quick Save** | `opt+up`, `opt+down` | Save project |
| **Copy** | `select+down` | Copy current item |
| **Paste** | `select+up` | Paste item |
| **Settings** | `opt+edit` | Open settings |

---

## Architecture & Performance

### Rendering Pipeline

**Native Format Rendering:**
- Detects framebuffer depth at startup (16-bit RGB565 or 32-bit ARGB8888)
- Allocates internal buffer matching native pixel format
- Colors are packed **once** during draw operations
- Blit phase is a raw `memcpy` with zero per-pixel conversion overhead

**Dirty Rectangle Optimization:**
- Tracks modified regions per frame (`min_x`, `min_y`, `max_x`, `max_y`)
- Adds safety padding (2px horizontal, 6px vertical) to prevent font artifacts
- Only modified regions are copied to framebuffer
- Full-screen updates only when necessary (screen transitions)

**VSync Prevention:**
- Uses `FBIO_WAITFORVSYNC` ioctl to synchronize with display refresh
- Eliminates horizontal tearing on waveform updates

### Protocol Handling

**SLIP Decoding:**
- Byte-stream protocol decoder (`slip.c`)
- Handles escaped characters and frame boundaries

**M8 Commands:**
| Command | Byte | Description |
|---------|------|-------------|
| Draw Rectangle | `0xFE` | Fill/clear operations |
| Draw Character | `0xFD` | Text rendering with font selection |
| Draw Waveform | `0xFC` | Oscilloscope visualization |
| System Info | `0xFF` | Font mode selection |

**Color Compression:**
- M8 uses "running status" to reduce bandwidth
- Last received RGB values persist between commands
- Only sends color when it changes

### Input Processing

**Event Loop:**
- Single-threaded with `poll()` multiplexing
- Non-blocking reads on serial and input file descriptors
- Immediate translation to M8 button bitmask
- Sub-millisecond latency from keypress to M8

**Button Bitmask Format:**
```
Bit 7: Left
Bit 6: Up
Bit 5: Down
Bit 4: Select
Bit 3: Start
Bit 2: Right
Bit 1: Opt
Bit 0: Edit
```

---

## Font System

Five embedded bitmap fonts with different offsets for hardware versions:

| Font | Model | Screen Offset | Text Offset | Waveform Height |
|------|-------|---------------|-------------|-----------------|
| `font1.h` | v1 Small | 0 | 0 | 20 |
| `font2.h` | v1 Large | 0 | 0 | 20 |
| `font3.h` | v2 Small | 0 | -3 | 21 |
| `font4.h` | v2 Large | 0 | -4 | 21 |
| `font5.h` | v2 Huge | -2 | -7 | 21 |

Fonts are selected automatically based on M8's `CMD_SYSTEM_INFO` response.

---

## Display Optimization Details

### Artifact Prevention

**Problem:** Text rendering with vertical offsets can leave "ghost" pixels when screens change.

**Solution:** 
- **Text/Cursors**: Drawn relative to font offset
- **Clears (Black/Background)**: Detected by color and treated as absolute coordinates
- **Space Character (ASCII 32)**: Explicitly draws background-colored rectangle

**Padding Strategy:**
- Horizontal: ±2px (handles character width variations)
- Vertical: ±6px (clears font descenders and offset mismatches)

### Bresenham Line Algorithm

Waveform rendering uses optimized Bresenham's algorithm:
- Integer-only math (no floating point)
- Single-pixel precision
- Clears previous waveform column before drawing new data

---

## Troubleshooting

### M8 Not Connecting

```bash
# Check if M8 is detected
dmesg | grep tty
# Should show: cdc_acm 1-1:1.0: ttyACM0: USB ACM device

# Check permissions
ls -l /dev/ttyACM0
# Should be in 'dialout' group

# Test serial connection
sudo screen /dev/ttyACM0 115200
# Press Ctrl-A, K to quit
```

### Display Issues

**Black Screen:**
```bash
# Check framebuffer
cat /dev/urandom > /dev/fb0
# Should show noise

# Check display info
fbset -i

# Try alternate framebuffer
framebuffer_device=/dev/fb1  # in config.ini
```

**Wrong Colors:**
- Supports RGB565 (16-bit) and ARGB8888 (32-bit)
- Automatically detects format via `ioctl`
- Check `fbset` output for `bits_per_pixel`

**Tearing/Artifacts:**
- Ensure VSync is working: some SPI displays don't support `FBIO_WAITFORVSYNC`
- Increase dirty rectangle padding in `display.c` if needed

### Input Not Working

```bash
# List all input devices
cat /proc/bus/input/devices

# Test input device
sudo evtest /dev/input/event4
# Press keys to verify events

# Check permissions
ls -l /dev/input/event4
# Should be in 'input' group
```

### Autoplay Not Executing

- Check `enabled=1` in `[autoplay]` section
- Verify sequence syntax (no trailing commas)
- Watch console output: shows "Autoplay: Executed 'command'"
- Ensure M8 is fully connected before start_delay expires

---

## Development

### Building for Cross-Compilation

**Raspberry Pi (ARM):**
```bash
# Install cross-compiler
sudo apt-get install gcc-arm-linux-gnueabihf musl-tools

# Modify Makefile
CC = arm-linux-gnueabihf-gcc
CFLAGS = -Wall -O3 -Isrc -static -march=armv7-a

make clean && make
```

**Static Binary Verification:**
```bash
file m8alt
# Should show: "statically linked"

ldd m8alt
# Should show: "not a dynamic executable"
```

### Code Structure

```
m8alt/
├── Makefile           # musl-gcc static build configuration
├── config.ini         # Runtime configuration
├── src/
│   ├── main.c         # Event loop, autoplay orchestration
│   ├── display.c/h    # Framebuffer rendering, dirty rects
│   ├── input.c/h      # Evdev keyboard/gamepad handling
│   ├── serial.c/h     # M8 protocol, SLIP decoding
│   ├── ini.c/h        # Config file parser
│   ├── slip.c/h       # SLIP protocol implementation
│   ├── common.h       # Shared types and globals
│   └── fonts/         # Embedded bitmap fonts (font1-5.h)
└── obj/               # Build artifacts (generated)
```

### Adding New Features

**New Autoplay Commands:**
1. Add command name to `key_to_mask()` in `main.c`
2. Define bit mask (reference M8 protocol docs)
3. Update config.ini comments

**New Display Optimizations:**
1. Modify dirty rectangle logic in `display.c`
2. Adjust padding values in `mark_dirty()`
3. Test with rapid screen transitions

**New Input Devices:**
1. Add device path to config.ini
2. No code changes needed (evdev is universal)

---

## Performance Benchmarks

**Raspberry Pi Zero 2W (1GHz Quad-Core ARM Cortex-A53):**
- Frame Time: ~2-4ms (depending on dirty region size)
- Input Latency: <1ms (evdev to M8)
- CPU Usage: ~15-25% (single core, during active rendering)
- Memory: ~8MB RSS

**Full Screen Redraw:**
- 320×240 @ 16bpp: ~1.2ms (memcpy of 150KB)
- 320×240 @ 32bpp: ~2.4ms (memcpy of 300KB)

**Typical Frame (Dirty Rect):**
- Text update: 0.3-0.8ms
- Waveform update: 0.5-1.5ms
- Cursor blink: 0.1-0.3ms

---

## Known Limitations

- **Single M8 Support**: Only one M8 connection at a time
- **No Audio Routing**: This is a display client only (use M8's USB audio)
- **No Recording**: Does not capture M8 screen output to file
- **Fixed Resolution**: 320×240 native, no scaling (performance optimization)
- **Linux Only**: Requires Linux framebuffer and evdev subsystems

---

## License

This project incorporates:
- **SLIP decoder**: MIT License (Marcin Borowicz)
- **INI parser**: MIT License (rxi)
- **Original m8c**: MIT License

See individual source files for detailed attribution.

---

## Credits

- **Original m8c**: laamaa and contributors
- **Dirtywave M8**: Timothy Lamb (trash80)
- **Embedded port**: Optimized for headless embedded deployment

---

## Support

**Issues & Questions:**
- Check this README first
- Review `config.ini` comments
- Enable autoplay logging for sequence debugging

**Contributing:**
- Performance improvements welcome
- New platform testing appreciated
- Keep static binary compatibility

---

## Changelog

### v3.0 (Current)
- ✨ Added autoplay system with combo key support
- 🔧 Static musl-libc compilation
- ⚡ Native framebuffer format rendering
- 🎯 Dirty rectangle optimization
- 🔄 VSync support
- 📝 Comprehensive documentation

### v2.0
- 🖥️ Direct framebuffer rendering
- ⌨️ Evdev input handling
- 🔌 SLIP protocol implementation

### v1.0
- 🎉 Initial embedded port from SDL version

---

**Happy M8 Tracking on Embedded Hardware!** 🎵🎛️
