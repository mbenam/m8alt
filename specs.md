# PROJECT CONTEXT: M8C Embedded – Unified Specification (v3.0 Optimized)

## 1. Project Overview

A lightweight, bare-metal C port of the **m8c** client for **Embedded Linux** (Raspberry Pi, Luckfox, etc.).
This version replaces the original SDL3 implementation with a highly optimized, direct-to-framebuffer renderer suitable for low-power ARM processors.

### Key Goals
- **Zero-Dependency:** No SDL, libusb, or X11/Wayland required.
- **High Performance:** Runs on minimal hardware (e.g., Pi Zero 2W, single-core ARMv7).
- **Direct Framebuffer Access:** Renders to `/dev/fb0`.
- **Latency Optimized:** Uses dirty rectangle tracking to minimize bus traffic.
- **Evdev Input:** Reads directly from `/dev/input/eventX`.

---

## 2. Architecture & Modules

The application is **single-threaded**. The main loop uses `poll()` to multiplex serial data and input events, while the display subsystem manages its own state and optimization logic.

---

## A. Display (`src/display.c`, `src/display.h`)

### 1. Rendering Pipeline (Native Buffer)
To eliminate conversion overhead during the critical flush phase, the renderer detects the framebuffer depth at startup:
- **Initialization:** Detects `bits_per_pixel` via `ioctl`.
- **Buffer Allocation:** Allocates an internal RAM buffer (`render_buffer`) matching the native screen format (16-bit RGB565 or 32-bit ARGB8888).
- **Draw Time:** Colors are packed into the native format *once* when drawing primitives.
- **Blit Time:** The flush operation is a raw `memcpy` (or optimized block copy), requiring zero math per pixel.

### 2. Dirty Rectangle Tracking
Instead of redrawing the full 320x240 screen every frame:
- The system tracks `min_x`, `min_y`, `max_x`, and `max_y` of any pixel modified during a frame.
- **Aggressive Padding:** A safety margin (2px horizontal, 6px vertical) is added to dirty regions to ensure font "tails" and vertical offsets are correctly cleared, preventing artifacts.
- **Optimization:** If no pixels change, `display_blit` returns immediately.

### 3. Artifact Prevention & Logic
- **Font Offsets:** Text is drawn with a vertical offset (defined in font headers).
- **Absolute vs. Relative Clears:**
  - **Text/Cursors:** Drawn relative to the font offset.
  - **Clears (Black/Background):** Detected by color (`0x00` or `global_bg_color`). These are treated as absolute coordinates to ensure the entire line/screen is wiped clean, fixing "ghosting" artifacts from previous screens.
- **Space Character (ASCII 32):** Explicitly draws a background-colored rectangle to erase underlying text.

### 4. VSync & Tearing
- Uses `ioctl(fb_fd, FBIO_WAITFORVSYNC, ...)` to synchronize updates with the display refresh rate, preventing horizontal tearing on rapid waveform updates.

### 5. Drawing Primitives
- **`display_draw_char`:** 
  - Inlined bitmap parsing (no helper function overhead).
  - Direct bit-shift extraction from font data.
  - Writes directly to the native pointer (pointer arithmetic) rather than array indexing.
- **`display_draw_rect`:**
  - Uses `memset` for black/clear operations for maximum speed.
  - Uses pointer increment loops for colored rectangles.
- **`display_draw_waveform`:**
  - Bresenham's line algorithm.
  - Clears only the specific column/area used by the previous frame's waveform before drawing the new one.

---

## B. Input (`src/input.c`, `src/input.h`)

### Method
- Reads `struct input_event` from `/dev/input/eventX`.
- Non-blocking read loop.

### Mapping
Converts Linux keycodes → M8 bitmask:
- **Bit 7:** Left
- **Bit 6:** Up
- **Bit 5:** Down
- **Bit 4:** Select (Shift)
- **Bit 3:** Start (Space)
- **Bit 2:** Right
- **Bit 1:** Opt (Ctrl)
- **Bit 0:** Edit (Alt)

### Behavior
- State changes trigger a Serial Write (`0x43` + byte).
- Debouncing is handled implicitly by the Linux kernel evdev driver.

---

## C. Serial / Protocol (`src/serial.c`, `src/slip.c`)

### Configuration
- **Device:** `/dev/ttyACM0` (Configurable via `config.ini`).
- **Settings:** 115200 baud, 8N1, Raw mode.

### Protocol Handling
- **SLIP Decoding:** Byte-stream decoding via `slip.c`.
- **Command Handling:**
  - `0xFE`: Draw Rectangle (Updates dirty rects).
  - `0xFD`: Draw Character (Updates dirty rects).
  - `0xFC`: Draw Oscilloscope (Updates dirty rects).
  - `0xFF`: System Info (Used to select Font offsets).
- **Stateful Colors:** Maintains the last received RGB value to optimize bandwidth (M8 sends compressed commands without color if it hasn't changed).

---

## D. Fonts (`src/fonts/`)

- Headers: `font1.h` through `font5.h`.
- Implementation: Included directly into `display.c` to allow the compiler to inline data access.
- **Note:** Bitmap headers are parsed manually in `display_draw_char` to skip the 54-byte BMP header and jump straight to pixel data.

---

## 3. Configuration (`config.ini`)

```ini
[system]
serial_device=/dev/ttyACM0
framebuffer_device=/dev/fb0
input_device=/dev/input/event0

[keyboard]
; Standard Linux Keycodes
key_up=103
key_down=108
key_left=105
key_right=106
key_select=42  ; Left Shift
key_start=57   ; Space
key_opt=29     ; Left Ctrl
key_edit=56    ; Left Alt
```

---

## 4. Compile & Run

### Dependencies
- Standard C Library (libc).
- Linux Headers (for `linux/fb.h`, `linux/input.h`).

### Build
```bash
make
```

### Execution
```bash
./m8alt
```
*(Requires read/write permissions for `/dev/fb0`, `/dev/ttyACM0`, and `/dev/input/eventX`)*.

