# PROJECT CONTEXT: M8C Embedded – Unified Specification (Combined v1 + v2.0)

## 1. Project Overview

A lightweight, bare-metal C port of the **m8c** client for **Embedded Linux** (Raspberry Pi 3B+, Zero 2W, Luckfox Lyra 128MB).  
This version replaces the original SDL3/libusb application with a minimal framebuffer and evdev-based implementation.

### Key Goals
- Fully remove heavy dependencies (SDL3, libusb, libserialport, rtmidi).
- Run on extremely low-end Linux hardware (as low as 128MB RAM).
- Render directly to `/dev/fb0` without X11/Wayland.
- Use evdev input directly (no SDL input).
- Communicate with the M8 over raw serial + SLIP.

### Removed Features (from original m8c)
- Audio routing  
- MIDI handling  
- Keyjazz  
- Screensaver  
- In-app Settings  
- Firmware flashing  

### Display
- Fixed 320×240 internal resolution.
- Centered on any framebuffer size.
- Direct software rendering (no hardware acceleration).
- Software double buffering.

### Audio (System-level, not in m8c)
- External I2S DAC (e.g., UDA1334A).
- Audio handled externally using `alsaloop`.

---

## 2. Architecture & Modules

The application is **single-threaded**.  
Main loop uses **`poll()`** to multiplex serial + input device events.

---

## A. Display (`src/display.c`, `src/display.h`)

### Rendering Method
- Memory-mapped framebuffer (`mmap` `/dev/fb0`).
- Internal buffer: `uint32_t render_buffer[320][240]` (ARGB8888).
- Buffer is blitted → framebuffer each frame.

### Color Format Handling
- Detects framebuffer format via `ioctl(FBIOGET_VSCREENINFO)`.
- Converts ARGB8888 →  
  - **RGB565**, or  
  - **BGRA/ARGB** (depending on FB ordering).

### Software Rasterizer
Implements:
- `draw_rect`
- `draw_char` (1-bit font bitmap)
- `draw_waveform` using **Bresenham line algorithm**

### Critical Fixes (from Version 2.0)
- **screen_offset_y logic** applied for rectangle erases.
- **Full-screen clear** triggered when M8 sends `(x=0,y=0)` rectangle.
- **Waveform cleanup**: background wipe of oscilloscope region before drawing.
- **Ghosting fixes** for bottom rows and waveform pane.

---

## B. Input (`src/input.c`, `src/input.h`)

### Method
- Reads `struct input_event` from `/dev/input/eventX`.

### Mapping
Converts Linux keycodes → M8 bitmask:
- Left = 0x80  
- Up = 0x40  
- Down = 0x20  
- Select = 0x10  
- Start = 0x08  
- Right = 0x04  
- Opt = 0x02  
- Edit = 0x01  

### Behavior
- Sends `0x43` (Control command) when any key changes.
- Uses mapping defined in `config.ini`.

---

## C. Serial / Protocol (`src/serial.c`, `src/slip.c`)

### Serial Setup
- Device: `/dev/ttyACM0`
- Configuration: **115200 baud, 8N1, raw**
- POSIX `termios`

### Protocol
- SLIP packet decoding via `slip.c`.
- Initial handshake:
  - `'D'` = Disconnect  
  - `'E'` = Enable Display  
  - `'R'` = Reset  

### Commands Supported
- `0xFE`: Draw Rectangle  
- `0xFD`: Draw Character  
- `0xFC`: Draw Oscilloscope  
- `0xFF`: System Info (distinguishes M8 Model:01 vs Model:02 → font offset)

### Stateful Color Fix (v2.0)
- Maintains **running color state**:
  static uint8_t last_r, last_g, last_b;
- Allows rectangles with no color data to reuse last color.
- Fixes “white bar” artifact.

---

## D. Fonts (`src/fonts/`)

### Source
- Uses original M8C font headers: `font1.h ... font5.h`.

### Implementation Notes
- **fonts.c is excluded** to avoid multiple definitions.
- `display.c` directly includes the header arrays.
- Bitmap parsing skips the **54-byte BMP header** embedded in the data.
- Fonts are 1-bit depth bitmaps.

---

## 3. Directory Structure

```text
m8c-embedded/
├── Makefile       (Excludes fonts.c → prevents duplicate symbols)
├── config.ini     (Serial/FB/Input paths + key mapping)
└── src/
    ├── main.c     (Main loop, config reader, poll() loop)
    ├── common.h   (Shared constants)
    ├── display.c  (Framebuffer rendering + fonts)
    ├── input.c    (Evdev → M8 bitmask)
    ├── serial.c   (SLIP + stateful protocol handler)
    ├── slip.c     (SLIP decoding)
    ├── ini.c      (Config parser)
    └── fonts/     (font1.h ... font5.h)
```

---

## 4. Configuration Format (`config.ini`)

```ini
[system]
serial_device=/dev/ttyACM0
framebuffer_device=/dev/fb0
; Prefer by-id for stability, but event0 is valid on Lyra
input_device=/dev/input/by-id/usb-Manufacturer-Product-event-kbd
; or:
; input_device=/dev/input/event0

[keyboard]
; Linux Input Event Codes (int)
key_up=103
key_down=108
key_left=105
key_right=106
key_select=42
key_start=57
key_opt=29
key_edit=56
```

---

## 5. Troubleshooting Checklist

### When reporting issues, include:

### A. Error Output
- Any compile-time errors
- Any runtime log messages

### B. Input Device Check
```
ls -l /dev/input/by-id/
```

### C. Framebuffer Check
```
fbset -i
```

### D. Serial Check
```
dmesg | grep tty
```
