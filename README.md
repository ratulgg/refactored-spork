# 🐄 Cow Muzzle Capture – ESP32‑S3 Live Camera + IR Temperature

A standalone device that captures a cow's muzzle photo together with
object & ambient temperature and serves it via a built‑in WiFi hotspot.
The device consists of an **ESP32‑S3 CAM board (Freenove)**, a **2.8" ST7789 TFT**
for live preview, an **MLX90614 IR temperature sensor**, and a **physical shutter button**.

When the button is pressed, the current JPEG frame and the temperature readings
are saved. Any device (phone, laptop, tablet) connected to the device's WiFi
can open a web page to view the latest capture and its temperatures.

---

## Hardware Components

| Component                   | Details                                      |
|-----------------------------|----------------------------------------------|
| **Main board**              | Freenove ESP32‑S3 WROOM CAM (OV3660, OPI PSRAM) |
| **Display**                 | 2.8" SPI TFT, ST7789 controller, 240×320, touch NOT used |
| **IR Temperature Sensor**   | MLX90614 (I²C, GY‑906 module)                |
| **Shutter button**          | Momentary push button (GPIO 39 ↔ GND, internal pull‑up) |
| **Power supply for TFT**    | External 3.3 V / 5 V (e.g., second ESP32 WROOM board's 5V pin) – see notes |

---

## Pin Connections

### TFT ↔ ESP32‑S3 (Logic Pins)

| TFT Pin | ESP32‑S3 GPIO | Description         |
|---------|---------------|---------------------|
| CS      | 14            | Chip Select         |
| DC      | 21            | Data/Command        |
| RST     | 41            | Reset               |
| MOSI    | 48            | Master Out Slave In |
| SCLK    | 38            | SPI Clock           |
| MISO    | NOT CONNECTED | (3‑wire SPI mode)   |

**Note:** The TFT's backlight (LED) is powered separately. The display module has an
on‑board regulator (U1) and the J1 jumper is open, so you can supply **5 V** to the
VCC pin – the regulator provides 3.3 V to the panel. Connect LED to 3.3 V or 5 V
depending on your module (3.3 V is safer).

### Camera Pin Mapping (Freenove ESP32‑S3 WROOM CAM)

| Camera Pin | ESP32‑S3 GPIO | Camera Pin | ESP32‑S3 GPIO |
|------------|---------------|------------|---------------|
| XCLK       | 15            | Y2 (D0)    | 11            |
| PCLK       | 13            | Y3 (D1)    | 9             |
| VSYNC      | 6             | Y4 (D2)    | 8             |
| HREF       | 7             | Y5 (D3)    | 10            |
| SIOD       | 4             | Y6 (D4)    | 12            |
| SIOC       | 5             | Y7 (D5)    | 18            |
| Y9 (D7)    | 16            | Y8 (D6)    | 17            |
| PWDN       | -1 (unused)   | RESET      | -1 (unused)   |

### IR Sensor (MLX90614) ↔ I²C

| Sensor Pin | ESP32‑S3 GPIO |
|------------|---------------|
| VCC        | 3.3 V         |
| GND        | GND           |
| SDA        | 3             |
| SCL        | 46            |

> **Pull‑ups:** The MLX90614 module usually includes 4.7 kΩ pull‑ups to 3.3 V.
> If your module doesn't have them, add external 4.7 kΩ resistors from SDA to 3.3 V
> and SCL to 3.3 V.

### Shutter Button

| Button Leg | ESP32‑S3 GPIO |
|------------|---------------|
| One leg    | 39            |
| Other leg  | GND           |

Internal pull‑up is enabled – the pin reads **LOW** when pressed.

### Power Connections

- **ESP32‑S3 CAM** is powered via its USB‑C port (5 V from computer or power bank).
- **TFT VCC** is connected to **5 V** (the board's USB 5 V pin or an external 5 V source like a second ESP32).
- **TFT LED** is connected to **3.3 V** (from the external supply's 3.3 V regulator, or the ESP32‑S3's 3.3 V if it can provide enough current – but using a separate supply eliminates flickering due to power drops).
- **All GNDs** must be tied together (ESP32‑S3, TFT, sensor, button).

> **Why separate power?** The camera initialisation draws a current spike that can
> cause the TFT backlight to flicker or the camera to fail if they share the same
> weak 3.3 V regulator. Using a separate 5 V source for the TFT (through its own
> regulator) solves this.

---

## ASCII Connection Diagram
