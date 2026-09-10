#pragma once

#include <cstdint>

namespace vipi {

enum class DisplayType {
    NONE = 0,
    SSD1306 = 1,
    ST7789 = 2
};

enum class DisplayState {
    BOOTING,
    CONNECTING,
    IDLE,
    WAKE,
    LISTENING,
    THINKING,
    SPEAKING,
    MUSIC,
    TIMER,
    ALARM,
    MUTED,
    WARNING,
    ERROR,
    OTA
};

struct DisplayConfig {
    bool enabled = true;
    DisplayType type = DisplayType::SSD1306;

    int width = 128;
    int height = 64;

    int offset_x = 0;
    int offset_y = 0;

    bool mirror_x = true;
    bool mirror_y = true;
    bool swap_xy = false;
    bool invert_color = false;

    int rotation = 0;
    uint8_t brightness = 80;

    // SSD1306 / I2C
    int oled_sda = 41;
    int oled_scl = 42;
    uint8_t oled_address = 0x3C;

    // ST7789 / SPI
    int spi_mosi = -1;
    int spi_clk = -1;
    int spi_dc = -1;
    int spi_rst = -1;
    int spi_cs = -1;
    int backlight_pin = -1;
    bool backlight_invert = false;
};

}  // namespace vipi
