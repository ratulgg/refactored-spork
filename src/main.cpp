#include <Arduino.h>
#include <LovyanGFX.hpp>
#include "esp_camera.h"
#include <TJpg_Decoder.h>

// =====================
// TFT pins (non‑conflicting)
// =====================
#define TFT_CS   14
#define TFT_DC   21
#define TFT_RST  47
#define TFT_MOSI 48
#define TFT_SCLK 38

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ST7789 _panel_instance;
    lgfx::Bus_SPI      _bus_instance;
    lgfx::Light_PWM    _light_instance;
public:
    LGFX(void) {
        {
            auto cfg = _bus_instance.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 20000000;
            cfg.freq_read  = 10000000;
            cfg.spi_3wire = true;
            cfg.pin_sclk = TFT_SCLK;
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = -1;
            cfg.pin_dc   = TFT_DC;
            _bus_instance.config(cfg);
            _panel_instance.setBus(&_bus_instance);
        }
        {
            auto cfg = _panel_instance.config();
            cfg.pin_cs   = TFT_CS;
            cfg.pin_rst  = TFT_RST;
            cfg.memory_width  = 240;
            cfg.memory_height = 320;
            cfg.panel_width   = 240;
            cfg.panel_height  = 320;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl = -1;
            _light_instance.config(cfg);
            _panel_instance.setLight(&_light_instance);
        }
        setPanel(&_panel_instance);
    }
};

LGFX tft;

// =====================
// Sprite (off‑screen canvas)
// =====================
lgfx::LGFX_Sprite sprite(&tft);   // 320x240 sprite for JPEG

// =====================
// Camera pins (unchanged)
// =====================
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

// JPEG decoder callback – writes decoded pixels into the sprite
bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
    sprite.pushRect(x, y, w, h, bitmap);
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
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.frame_size = FRAMESIZE_QVGA;      // 320x240
    config.pixel_format = PIXFORMAT_JPEG;    // <-- use JPEG!
    config.jpeg_quality = 12;                // high quality
    config.fb_count = 2;                     // double buffer

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed 0x%x\n", err);
        while (1) delay(1000);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("TFT + Camera (JPEG mode)");

    // ---- TFT ----
    tft.init();
    tft.setRotation(2);          // portrait, upright (same as FIFA test)
    tft.setSwapBytes(true);      // ST7789 color swap (known good)
    tft.fillScreen(TFT_BLACK);

    // ---- Sprite (off‑screen canvas) ----
    sprite.setColorDepth(16);    // RGB565
    sprite.createSprite(320, 240);   // QVGA size
    sprite.setSwapBytes(true);

    // ---- JPEG decoder ----
    TJpgDec.setJpgScale(1);      // no scaling
    TJpgDec.setCallback(tft_output);

    // ---- Camera ----
    setup_camera();
    Serial.println("Camera ready");
}

void loop() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        delay(1);
        return;
    }

    // Draw JPEG onto the sprite (using TJpgDec)
    TJpgDec.drawJpg(0, 0, fb->buf, fb->len);

    // Rotate the sprite 90° and push to the display
    // This maps the 320x240 sprite onto the 240x320 screen
    sprite.pushRotateZoom(&tft, 120, 160, 90, 1.0, 1.0);

    esp_camera_fb_return(fb);
    delay(5);
}