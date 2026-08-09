# Mouse Jiggler — Chaotic BLE HID Mouse

A mischievous ESP32 WROOM / D0WD-V3 device that emulates a Bluetooth mouse and sends random, aggressive movement patterns every ~30 seconds. Perfect for keeping your screen awake, confusing screen lock policies, or just having some fun with your productivity apps.

**No external components required** — works standalone on the ESP32 dev board.

---

## Features

- **5 Attack Patterns**: Circle, Figure-8, Zigzag, Chaos, Spiral — all extreme and aggressive
- **Extreme Movement Range**: 100–200+ pixels per pattern (scales to your monitor size)
- **BLE HID Compliant**: Pairs like a real Bluetooth mouse (Windows, macOS, Linux)
- **Serial Control**: Kill/resume, check status, reset counter via USB UART
- **Status LED**: Visual feedback — slow blink (advertising), solid (connected+running), fast blink (paused)
- **Natural Randomization**: ±5 second variance prevents detection by activity monitoring

---

## Hardware

| Component | Details |
|-----------|---------|
| **Microcontroller** | ESP32 WROOM or D0WD-V3 (30-pin or 38-pin) |
| **Flash** | 4 MB (standard) |
| **RAM** | 520 KB SRAM |
| **Connectivity** | Bluetooth 5.0 LE |
| **GPIO Used** | Pin 2 (LED status indicator) |
| **Power** | USB 5V via micro-USB or 3.3V external |

No external components, capacitors, resistors, or level shifters needed.

---

## Installation

### 1. Upload Firmware

Use **Arduino IDE** with ESP32 board support:

1. Install the **ESP32 Arduino Core**: Boards Manager → Search "ESP32" → Install by Espressif Systems
2. Select Board: `ESP32 Dev Module` (or `ESP32 DEVKITV1` depending on your variant)
3. Copy the firmware code into Arduino IDE
4. Install required libraries via Arduino Library Manager:
   - `BLE Arduino` (built-in, no install needed)
   - `HID Arduino` (built-in, no install needed)
5. Connect ESP32 via USB
6. Click **Upload**

Alternative (PlatformIO):
```bash
pio project init --board esp32dev
# Copy firmware into src/main.cpp
pio run -t upload
pio device monitor  # View serial output
```

### 2. Pair with Host

- ESP32 advertises as **`MouseJiggler`** over BLE
- Open Bluetooth Settings on your device
- Select "MouseJiggler" and pair (no PIN required)
- Device should connect automatically after pairing

---

## Usage

### Serial Commands

Open a serial monitor at **115200 baud** and send commands:

| Command | Effect |
|---------|--------|
| `kill` | Toggle pause/resume (LED fast-blinks when paused) |
| `status` | Print connection state, running status, pattern count |
| `count` | Show total patterns executed |
| `reset` | Reset pattern counter to 0 |

Example:
```
>>> status
╔══════════════════════════════════════╗
║ BLE:     Connected                   ║
║ State:   RUNNING                     ║
║ Patterns: 12                         ║
║ Next in: ~18s                        ║
╚══════════════════════════════════════╝

>>> kill
[CMD] Jiggler PAUSED

>>> kill
[CMD] Jiggler RESUMED
```

### LED Indicators

| Pattern | Meaning |
|---------|---------|
| 🔴 Slow blink (1 Hz) | Advertising (waiting to pair) |
| 🟢 Solid on | Connected + actively jigging |
| 🟡 Fast blink (5 Hz) | Paused (waiting for `kill` command to resume) |

---

## Pattern Details

### Circle
Smooth circular motion, ~200px diameter. Mathematically elegant and predictable — good for steady screen activity.

**Characteristics**: 40 steps, 20ms between steps, returns to center

### Figure-8
Lissajous curve (sine + 2× sine), ~200px wide. Hypnotic and impossible to ignore.

**Characteristics**: 48 steps, 18ms between steps, ~90px × 180px amplitude

### Zigzag
Sharp diagonal bursts across the screen. **Most aggressive pattern.**

**Characteristics**: 4–9 random legs, 120px per leg, instant direction changes

### Chaos
Pure random fling bursts with irregular timing. Unpredictable and frantic.

**Characteristics**: 6–13 random ±127px moves, 25–90ms delays between bursts

### Spiral
Expanding spiral that grows outward then snaps back. Visually striking.

**Characteristics**: 36-step outward spiral (grows to ~216px), aggressive expansion rate

---

## Timing

- **Jiggle Interval**: 30 seconds ± 5 seconds (randomized to avoid detection)
- **Pattern Duration**: 2–4 seconds per pattern (varies by type)
- **HID Report Rate**: 8ms between each mouse move (125 Hz refresh)

Example timeline:
```
00:00 → Pattern fires (2s)
00:02 → Idle
...
00:25 → Idle
00:30 → Pattern fires again
00:32 → Idle
...
01:00 → Pattern fires
```

---

## Configuration (Source Code)

Edit these constants in the firmware to customize behavior:

```cpp
#define LED_PIN    2      // GPIO pin for status LED (change if pin 2 conflicts)
#define INTERVAL   30000  // Milliseconds between jiggles (30s = 30000ms)
#define VARIANCE   5000   // ±5s randomness (±5000ms)
```

### Adjust Pattern Extremes

Look for these lines in each pattern function:

- **Circle**: `const float radius = 150.0;` (increase for bigger circles)
- **Figure-8**: `const float A = 180.0; const float B = 90.0;` (larger A/B = wilder curves)
- **Zigzag**: `const int legSize = 120;` (larger value = longer zigs)
- **Chaos**: `int8_t dx = (int8_t)random(-127, 128);` (already maxed out)
- **Spiral**: `float radius = 6.0 * i;` (larger multiplier = faster expansion)

Higher values = more aggressive, more likely to trigger anti-idleness detection.

---

## Safety & Legality

⚠️ **Use responsibly:**

- Some organizations actively detect and block input spoofing (mouse jiggling, keyboard injection)
- Using this to evade security policies or surveillance software may violate:
  - Employer policies
  - Terms of Service (Teams, Slack, etc.)
  - Local computer-use agreements
- **Use only on devices you own or have permission to run code on**

This project is educational and for personal use only.

---

## Troubleshooting

### Device doesn't advertise
- Check that pin 2 is free on your ESP32 variant
- Verify power (LED should blink slowly if powered)
- Try pressing the BOOT button and holding it while plugging in USB

### Patterns don't fire
- Serial output shows "Connected" but no jiggles? Check `status` — device might be paused (`PAUSED` state)
- Send `kill` to resume
- Verify no other HID devices are interfering with BLE pairing

### Moves too small
- Current firmware is tuned for *extreme* 100–200px ranges
- If moves feel weak, try:
  - Increasing `INTERVAL` to allow longer patterns
  - Adjusting pattern constants (see Configuration)
  - Checking that Bluetooth connection is stable (re-pair if needed)

### Connection drops repeatedly
- Some Bluetooth chipsets have interference issues
- Try moving away from WiFi routers, USB 3.0 hubs, or other wireless devices
- Re-pair the device from scratch (forget + pair again)

### Serial monitor not showing output
- Verify baud rate is **115200** (not 9600)
- Try pressing the RESET button on the ESP32
- Check USB cable is data-capable (not charge-only)

---

## Project Status

**Current Version**: 1.0 (Extreme Mode)  
**Last Updated**: 2026  
**License**: Open Source (MIT)

---

## Contributing

Found a bug or have a pattern idea? Contributions welcome. Fork, modify, test, and submit a PR.

---

## Disclaimer

This project is for educational purposes and personal entertainment. The author is not responsible for:
- Misuse in workplace or restricted environments
- Unintended consequences of screen-activity detection evasion
- Any disruption caused by mouse input spoofing

Use wisely. 🖱️✨
