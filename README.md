# Live Camera Feed on ST7789 TFT (ESP32‑S3 CAM)

This project streams a **live view from an OV3660 camera** directly to a **2.8" ST7789 SPI TFT display**, using a single **ESP32‑S3 CAM board** (Freenove variant).  
The display is powered externally by a second ESP32 (WROOM) that only provides 3.3V – its logic is not used.

The final setup gives a **low‑latency, full‑screen, flicker‑free video** with correct colours and orientation.

---

## Hardware Required

| Component | Notes |
|-----------|-------|
| **ESP32‑S3 CAM** (Freenove, AI‑Thinker or similar) | Must have OV3660 camera and OPI PSRAM |
| **2.8" TFT LCD** with ST7789 driver (240×320) | SPI interface |
| **Power supply for the TFT** | A second ESP32 board (WROOM) or any regulated 3.3V source capable of ~150 mA |
| Jumper wires | Female‑to‑female for the TFT, breadboard optional |
| **Common ground** | All GNDs **must** be connected together |

---

## Pin Connections

### TFT → ESP32‑S3 CAM (Logic Pins)

| TFT Pin | ESP32‑S3 GPIO | Notes |
|---------|----------------|-------|
| CS      | 14             | Chip Select |
| DC      | 21             | Data / Command |
| RST     | 47             | Reset |
| MOSI    | 48             | SPI Data |
| SCLK    | 38             | SPI Clock |

These pins were chosen because they **do not conflict** with the camera’s parallel data bus, USB, or PSRAM.

### Power Wiring

| TFT Pin | Power Source |
|---------|---------------|
| VCC     | 3.3V output of the second ESP32 (WROOM) or external regulator |
| LED     | 3.3V (can share VCC) – backlight always on |
| GND     | Connect to the **same GND** as the ESP32‑S3 CAM |

**Important:**  
- The ESP32‑S3 CAM board’s own 3.3V regulator may not have enough spare current for the TFT backlight. That’s why we use a separate 3.3V source (a second ESP32 board is convenient).  
- Only **GND** must be tied between the two boards – no other connections are needed to the second ESP32.

### Camera Pin Mapping (Freenove ESP32‑S3 WROOM CAM)

If your board is different, adjust these definitions in `setup_camera()`:
