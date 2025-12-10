# PROJECT CONTEXT: M8C Embedded (Linux Framebuffer Version)

## 1. Project Overview
We have ported **m8c** (a client for the Dirtywave M8 Tracker) from a heavy, cross-platform SDL3/libusb application to a lightweight, bare-metal C application optimized for embedded Linux (Raspberry Pi 3B+ / Zero 2W).

**Key Constraints & Scope:**
*   **Target:** Embedded Linux (headless or framebuffer console).
*   **Libraries Removed:** SDL3, libusb, libserialport, rtmidi.
*   **Features Removed:** Audio routing, MIDI handling, Keyjazz, Screensaver, In-App Settings, Firmware flashing.
*   **Video:** Direct rendering to Linux Framebuffer (`/dev/fb0`). Fixed 320x240 internal resolution (centered).
*   **Input:** Direct reading from Linux Input Subsystem (`/dev/input/eventX`).
*   **Config:** All settings via `config.ini` (no runtime UI).

## 2. Architecture & Modules

The application is single-threaded, utilizing a main loop with `poll()` to handle I/O multiplexing.

### A. Display (`src/display.c`, `src/display.h`)
*   **Technique:** Memory Mapped I/O (`mmap`) on `/dev/fb0`.
*   **Buffering:** Uses **Software Double Buffering**.
    *   `render_buffer`: A 320x240 array of `uint32_t` (ARGB8888) in heap memory.
    *   Drawing commands write to `render_buffer`.
    *   `display_blit()` converts and copies `render_buffer` to the actual framebuffer memory.
*   **Color Depth:** Automatically detects 16-bit (RGB565) or 32-bit (ARGB/BGRA) via `ioctl(FBIOGET_VSCREENINFO)` and handles pixel conversion during the blit.
*   **Primitives:** Implements software rasterization for:
    *   `draw_rect` (Solid color fill).
    *   `draw_char` (1-bit bitmap font rendering).
    *   `draw_waveform` (Bresenham's line algorithm for the oscilloscope).

### B. Input (`src/input.c`, `src/input.h`)
*   **Technique:** Reads `struct input_event` from `/dev/input/eventX` (evdev).
*   **Logic:** Maintains a standard M8 bitmask.
*   **Bitmask:** Left=0x80, Up=0x40, Down=0x20, Select=0x10, Start=0x08, Right=0x04, Opt=0x02, Edit=0x01.
*   **Mapping:** Maps generic Linux Key Codes (defined in `config.ini`) to the M8 bitmask.

### C. Serial / Protocol (`src/serial.c`, `src/slip.c`)
*   **Technique:** POSIX `termios` for serial communication (`/dev/ttyACM0`).
*   **Settings:** 115200 baud, 8N1, Raw mode.
*   **Protocol:** Uses **SLIP** (Serial Line Internet Protocol) to packetize data.
    *   Reuses `slip.c` and `slip.h` from the original codebase.
*   **Handshake:** On connection, sends 'D' (Disconnect), 'E' (Enable Display), 'R' (Reset).
*   **Incoming Commands:**
    *   `0xFE`: Draw Rectangle.
    *   `0xFD`: Draw Character.
    *   `0xFC`: Draw Oscilloscope Waveform.
    *   `0xFF`: System Info (Used to detect M8 Model:01 vs Model:02 and set font offset).

### D. Fonts (`src/fonts/`)
*   **Source:** Uses the original M8C header files (`font1.h` through `font5.h`).
*   **Implementation Detail:**
    *   **IMPORTANT:** We do **not** use `fonts.c` to avoid "multiple definition" linker errors.
    *   `display.c` includes the headers directly.
    *   Bitmap parsing is done manually by skipping the 54-byte BMP header found in the raw byte arrays and reading bits (1-bit depth).

## 3. Directory Structure

m8c-embedded/
├── Makefile (Standard gcc build)
├── config.ini (User settings)
└── src/
├─── main.c (Main loop, config parsing, polling)
├─── common.h (Shared structs/constants)
├─── display.c (Framebuffer logic)
├─── input.c (Evdev logic)
├─── serial.c (Termios/Protocol logic)
├─── ini.c (Config parser lib)
├─── slip.c (SLIP decoding lib)
└─── fonts/ (Headers only: font1.h ... font5.h)

## 4. Configuration Format (`config.ini`)
```ini
[system]
serial_device=/dev/ttyACM0
framebuffer_device=/dev/fb0
input_device=/dev/input/by-id/usb-Manufacturer-Product-event-kbd

[keyboard]
; Linux Input Event Codes (int)
key_up=103
key_down=108
...

## 5. Troubleshooting Data
If issues arise, please provide:
The Error: Copy-paste compiler errors or runtime output.
Input Device Check: Output of ls -l /dev/input/by-id/.
Framebuffer Check: Output of fbset -i.
Serial Check: Output of dmesg | grep tty.


# PROJECT CONTEXT: M8C Embedded (Final Working State) (Version 2.0)

## 1. Project Overview
A lightweight, bare-metal C port of the **m8c** client for Embedded Linux (Raspberry Pi / Luckfox Lyra). It renders the Dirtywave M8 interface directly to the framebuffer without SDL or heavy libraries.

**Hardware Target:**
*   **Device:** Luckfox Lyra (128MB RAM) / Raspberry Pi Zero 2W.
*   **Screen:** 320x240 (or centered on larger screens) via `/dev/fb0`.
*   **Audio:** External I2S DAC (UDA1334A) handling 44.1kHz native via `alsaloop`.

## 2. Architecture & Modules

### A. Display (`src/display.c`)
*   **Method:** Memory Mapped (`mmap`) `/dev/fb0` with software double-buffering (`malloc` buffer -> `memcpy` to FB).
*   **Format:** Internally **ARGB8888**. Auto-converts to **RGB565** or **BGRA** based on `ioctl` screen info.
*   **Critical Logic (Ghosting Fixes):**
    *   **Rectangles:** Applies `screen_offset_y` (from font) to align erase operations with text.
    *   **Full Clear:** Detects `x=0, y=0` clears and forces a **full buffer wipe** (ignoring offsets) to prevent bottom-edge ghosting.
    *   **Waveform:** Manually wipes the top-right waveform area using the global background color before drawing new lines (Bresenham).

### B. Serial Protocol (`src/serial.c`)
*   **Method:** `termios` on `/dev/ttyACM0` (115200 8N1 Raw).
*   **SLIP:** Uses `slip.c` to decode packets.
*   **Critical Logic (Color Fixes):**
    *   **Running Status:** Uses `static uint8_t last_r, last_g, last_b` to persist color state between commands. This allows "Draw Rect" commands with no color data to use the previous color correctly (fixing the "white bar" artifact).

### C. Input (`src/input.c`)
*   **Method:** Reads `struct input_event` from `/dev/input/eventX` (evdev).
*   **Protocol:** Maps Linux keys to the M8 bitmask (Left=0x80, Up=0x40, etc.) and sends `0x43` (Control) byte via serial.

## 3. Directory Structure
```text
m8c-embedded/
├── Makefile       (Excludes fonts.c to avoid dup symbols)
├── config.ini     (Keys, FB path, Input path)
└── src/
    ├── main.c     (Main loop, poll())
    ├── display.c  (FB logic, Font headers included here)
    ├── serial.c   (Stateful command parser)
    ├── input.c    (Evdev)
    ├── ini.c      (Config parser)
    ├── slip.c     (Packet decoder)
    └── fonts/     (Headers only: font1.h ... font5.h)
```

## 4. Configuration (`config.ini`)
```ini
[system]
serial_device=/dev/ttyACM0
framebuffer_device=/dev/fb0
input_device=/dev/input/event0

[keyboard]
key_up=103
key_down=108
key_left=105
key_right=106
key_select=42
key_start=57
key_opt=29
key_edit=56
```