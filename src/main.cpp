#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "esp_camera.h"
#include <TJpg_Decoder.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <WiFi.h>
#include <WebServer.h>

// =====================
// WiFi Access Point settings
// =====================
const char* ap_ssid = "CowMuzzleCam";      // Name of the WiFi you'll see on your phone
const char* ap_password = nullptr;          // nullptr = open network (no password)
                                            // Change to "12345678" if you want a password

WebServer server(80);                       // Web server on port 80

// =====================
// TFT – 3‑wire SPI, no MISO, no backlight PWM
// =====================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;
public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 20000000;
            cfg.freq_read  = 0;
            cfg.spi_3wire = true;
            cfg.pin_sclk = 38;
            cfg.pin_mosi = 48;
            cfg.pin_miso = -1;
            cfg.pin_dc   = 21;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = 14;
            cfg.pin_rst  = 41;
            cfg.memory_width  = 240;
            cfg.memory_height = 320;
            cfg.panel_width   = 240;
            cfg.panel_height  = 320;
            _panel_instance.config(cfg);
        }
        setPanel(&_panel_instance);
    }
};

LGFX tft;
lgfx::LGFX_Sprite sprite(&tft);

// Camera pins (Freenove ESP32‑S3 CAM)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     15
#define SIOD_GPIO_NUM     4
#define SIOC_GPIO_NUM     5
#define Y9_GPIO_NUM       16
#define Y8_GPIO_NUM       17
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       12
#define Y5_GPIO_NUM       10
#define Y4_GPIO_NUM       8
#define Y3_GPIO_NUM       9
#define Y2_GPIO_NUM       11
#define VSYNC_GPIO_NUM    6
#define HREF_GPIO_NUM     7
#define PCLK_GPIO_NUM     13

// Physical button
#define BUTTON_PIN 39

// IR sensor I2C pins
#define I2C_SDA 3
#define I2C_SCL 46

Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// Last captured image and temperatures (stored for the web server)
uint8_t* capturedJpeg = nullptr;
size_t   capturedJpegLen = 0;
float    capturedObjTemp = 0.0;
float    capturedAmbTemp = 0.0;
bool     newCaptureAvailable = false;

// JPEG decoder callback – writes into the landscape sprite
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    sprite.pushImage(x, y, w, h, bitmap);
    return 1;
}

void setup_camera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_QVGA;
    config.pixel_format = PIXFORMAT_JPEG;
    config.jpeg_quality = 12;
    config.fb_count = 2;

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed 0x%x\n", err);
        while (1) delay(1000);
    }
}

// =====================
// Visual Shutter Button (Sony Ericsson style, drawn directly on TFT)
// =====================
const int btn_cx = 210;
const int btn_cy = 280;
const int btn_radius = 35;
const int inner_radius = 29;
const int dot_radius = 7;
bool buttonVisualState = false;

void drawShutterButton() {
    tft.fillCircle(btn_cx, btn_cy, btn_radius, TFT_DARKGREY);
    tft.fillCircle(btn_cx, btn_cy, inner_radius, TFT_LIGHTGREY);
    tft.fillCircle(btn_cx, btn_cy, inner_radius-2, 0x5AEB);
    if (!buttonVisualState) {
        tft.fillCircle(btn_cx, btn_cy, dot_radius, TFT_RED);
    } else {
        tft.fillCircle(btn_cx, btn_cy, dot_radius, 0xF800);
        tft.drawCircle(btn_cx, btn_cy, btn_radius, TFT_WHITE);
    }
}

// =====================
// Temperature overlay (drawn directly on TFT)
// =====================
void drawTemperature(float objTemp, float ambTemp) {
    tft.fillRect(5, 5, 130, 40, 0x0000);               // black background
    tft.drawRect(4, 4, 132, 42, TFT_WHITE);            // white border
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.setTextSize(2);
    tft.setCursor(10, 10);
    tft.printf("Obj: %.1fC", objTemp);
    tft.setCursor(10, 28);
    tft.printf("Amb: %.1fC", ambTemp);
}

// =====================
// Web Server Handlers
// =====================
void handleRoot() {
    String html = "<!DOCTYPE html><html><head><title>Cow Muzzle Capture</title></head><body>";
    html += "<h1>Latest Capture</h1>";
    if (newCaptureAvailable) {
        html += "<p><b>Object Temp:</b> " + String(capturedObjTemp, 1) + " °C</p>";
        html += "<p><b>Ambient Temp:</b> " + String(capturedAmbTemp, 1) + " °C</p>";
        html += "<img src='/image' width='320'><br>";
        html += "<p><a href='/'>Refresh</a></p>";
    } else {
        html += "<p>No capture yet. Press the button on the device.</p>";
    }
    html += "</body></html>";
    server.send(200, "text/html", html);
}

void handleImage() {
    if (!newCaptureAvailable || !capturedJpeg) {
        server.send(404, "text/plain", "No image available");
        return;
    }
    server.sendHeader("Content-Type", "image/jpeg");
    server.send_P(200, "image/jpeg", (const char*)capturedJpeg, capturedJpegLen);
}

// =====================
void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("Cow Muzzle Capture – Access Point Mode");

    // TFT init
    tft.init();
    tft.setRotation(2);
    tft.setSwapBytes(true);
    tft.fillScreen(TFT_BLACK);

    // Off‑screen sprite for JPEG decoding
    sprite.setColorDepth(16);
    sprite.createSprite(320, 240);
    sprite.setSwapBytes(true);

    // JPEG decoder
    TJpgDec.setJpgScale(1);
    TJpgDec.setCallback(tft_output);

    // Camera
    setup_camera();
    Serial.println("Camera ready");

    // IR sensor
    Wire.begin(I2C_SDA, I2C_SCL);
    if (!mlx.begin()) {
        Serial.println("MLX90614 not found – check wiring!");
    } else {
        Serial.println("IR sensor ready");
    }

    // Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // ----- Start WiFi Access Point -----
    WiFi.softAP(ap_ssid, ap_password);
    Serial.print("Access Point SSID: ");
    Serial.println(ap_ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.softAPIP());   // Always 192.168.4.1

    // Configure web server routes
    server.on("/", handleRoot);
    server.on("/image", handleImage);
    server.begin();
    Serial.println("HTTP server started. Connect to the WiFi and visit http://192.168.4.1");
}

void loop() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        delay(1);
        server.handleClient();   // still serve web clients even if camera fails temporarily
        return;
    }

    // Decode JPEG into the landscape sprite, then rotate and push to display
    TJpgDec.drawJpg(0, 0, fb->buf, fb->len);
    sprite.pushRotateZoom(&tft, 120, 160, 90, 1.0, 1.0);

    // Read temperatures
    float objTemp = mlx.readObjectTempC();
    float ambTemp = mlx.readAmbientTempC();

    // Draw overlays directly on TFT
    drawTemperature(objTemp, ambTemp);

    bool btnState = (digitalRead(BUTTON_PIN) == LOW);
    buttonVisualState = btnState;
    drawShutterButton();

    // Handle button press (falling edge)
    static bool lastBtnState = false;
    if (btnState && !lastBtnState) {
        // Free old capture
        if (capturedJpeg) {
            free(capturedJpeg);
            capturedJpeg = nullptr;
        }

        // Save the current JPEG frame
        if (fb && fb->buf && fb->len > 0) {
            capturedJpeg = (uint8_t*)ps_malloc(fb->len);   // use PSRAM
            if (capturedJpeg) {
                memcpy(capturedJpeg, fb->buf, fb->len);
                capturedJpegLen = fb->len;
                capturedObjTemp = objTemp;
                capturedAmbTemp = ambTemp;
                newCaptureAvailable = true;
            }
        }

        Serial.printf("Shutter! Obj=%.1fC, Amb=%.1fC, JPEG=%u bytes\n",
                      objTemp, ambTemp, capturedJpegLen);

        // Brief visual feedback
        tft.fillRect(60, 120, 120, 30, TFT_BLACK);
        tft.setCursor(65, 125);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.setTextSize(2);
        tft.print("SENT!");
        delay(800);
    }
    lastBtnState = btnState;

    // Handle incoming web browser requests
    server.handleClient();

    esp_camera_fb_return(fb);
    delay(5);
}