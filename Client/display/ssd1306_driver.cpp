#include "ssd1306_driver.h"

#include <algorithm>
#include <cstring>

#include "ssd1306_font5x7.h"

#ifdef ARDUINO
#include <Wire.h>
#endif

namespace vipi {

bool Ssd1306Driver::begin(const DisplayConfig &config) {
    config_ = config;

    if (config_.width != WIDTH ||
        config_.height != HEIGHT ||
        config_.oled_sda < 0 ||
        config_.oled_scl < 0) {
        ready_ = false;
        return false;
    }

#ifdef ARDUINO
    Wire.begin(config_.oled_sda, config_.oled_scl);
    Wire.setClock(400000);

    const uint8_t init_sequence[] = {
        0xAE,
        0xD5, 0x80,
        0xA8, 0x3F,
        0xD3, 0x00,
        0x40,
        0x8D, 0x14,
        0x20, 0x00,
        static_cast<uint8_t>(config_.mirror_x ? 0xA1 : 0xA0),
        static_cast<uint8_t>(config_.mirror_y ? 0xC8 : 0xC0),
        0xDA, 0x12,
        0x81, 0xCF,
        0xD9, 0xF1,
        0xDB, 0x40,
        0xA4,
        static_cast<uint8_t>(config_.invert_color ? 0xA7 : 0xA6),
        0x2E,
        0xAF
    };

    for (uint8_t command : init_sequence) {
        if (!write_command_(command)) {
            ready_ = false;
            return false;
        }
    }
#endif

    ready_ = true;
    clear();
    present();
    set_brightness(config_.brightness);

    return true;
}

bool Ssd1306Driver::write_command_(uint8_t command) {
#ifdef ARDUINO
    Wire.beginTransmission(config_.oled_address);
    Wire.write(0x00);
    Wire.write(command);
    return Wire.endTransmission() == 0;
#else
    (void)command;
    return true;
#endif
}

bool Ssd1306Driver::write_data_(
    const uint8_t *data,
    size_t length
) {
#ifdef ARDUINO
    while (length > 0) {
        const size_t chunk = std::min<size_t>(length, 16);

        Wire.beginTransmission(config_.oled_address);
        Wire.write(0x40);
        Wire.write(data, chunk);

        if (Wire.endTransmission() != 0) {
            return false;
        }

        data += chunk;
        length -= chunk;
    }
#else
    (void)data;
    (void)length;
#endif

    return true;
}

void Ssd1306Driver::clear() {
    buffer_.fill(0);
}

void Ssd1306Driver::present() {
    if (!ready_) {
        return;
    }

    write_command_(0x21);
    write_command_(0);
    write_command_(WIDTH - 1);

    write_command_(0x22);
    write_command_(0);
    write_command_((HEIGHT / 8) - 1);

    write_data_(buffer_.data(), buffer_.size());
}

void Ssd1306Driver::set_brightness(uint8_t percent) {
    config_.brightness = std::min<uint8_t>(percent, 100);

    const uint8_t contrast = static_cast<uint8_t>(
        (static_cast<uint16_t>(config_.brightness) * 255) / 100
    );

    write_command_(0x81);
    write_command_(contrast);
}

void Ssd1306Driver::draw_pixel_(
    int x,
    int y,
    bool on
) {
    if (x < 0 || x >= WIDTH ||
        y < 0 || y >= HEIGHT) {
        return;
    }

    const int index = x + (y / 8) * WIDTH;
    const uint8_t mask = 1U << (y & 7);

    if (on) {
        buffer_[index] |= mask;
    } else {
        buffer_[index] &= ~mask;
    }
}

void Ssd1306Driver::draw_icon(
    int x,
    int y,
    const uint8_t *bitmap,
    int width,
    int height
) {
    if (!ready_ || bitmap == nullptr) {
        return;
    }

    const int bytes_per_row = (width + 7) / 8;

    for (int row = 0; row < height; ++row) {
        for (int col = 0; col < width; ++col) {
            const int byte_index =
                row * bytes_per_row + (col / 8);

            const uint8_t bit =
                1U << (col & 7);

            if (bitmap[byte_index] & bit) {
                draw_pixel_(x + col, y + row);
            }
        }
    }
}

void Ssd1306Driver::draw_char_(
    int x,
    int y,
    char c,
    uint8_t size
) {
    if (c < 0x20 || c > 0x7E) {
        c = '?';
    }

    const uint8_t *glyph = FONT_5X7[c - 0x20];

    for (int col = 0; col < 5; ++col) {
        uint8_t bits = glyph[col];

        for (int row = 0; row < 7; ++row) {
            if (bits & (1U << row)) {
                for (int sx = 0; sx < size; ++sx) {
                    for (int sy = 0; sy < size; ++sy) {
                        draw_pixel_(
                            x + col * size + sx,
                            y + row * size + sy
                        );
                    }
                }
            }
        }
    }
}

int Ssd1306Driver::text_width_(
    const std::string &text,
    uint8_t size
) const {
    return static_cast<int>(text.size()) * 6 * size;
}

void Ssd1306Driver::draw_text(
    int x,
    int y,
    const std::string &text,
    uint8_t size,
    bool centered
) {
    if (!ready_ || size == 0) {
        return;
    }

    int cursor_x = x;

    if (centered) {
        cursor_x = x - text_width_(text, size) / 2;
    }

    for (char c : text) {
        draw_char_(cursor_x, y, c, size);
        cursor_x += 6 * size;

        if (cursor_x >= WIDTH) {
            break;
        }
    }
}

void Ssd1306Driver::draw_progress(
    int x,
    int y,
    int width,
    int height,
    float ratio
) {
    ratio = std::clamp(ratio, 0.0f, 1.0f);

    for (int px = 0; px < width; ++px) {
        draw_pixel_(x + px, y);
        draw_pixel_(x + px, y + height - 1);
    }

    for (int py = 0; py < height; ++py) {
        draw_pixel_(x, y + py);
        draw_pixel_(x + width - 1, y + py);
    }

    const int fill_width = static_cast<int>(
        (width - 4) * ratio
    );

    for (int px = 0; px < fill_width; ++px) {
        for (int py = 2; py < height - 2; ++py) {
            draw_pixel_(x + 2 + px, y + py);
        }
    }
}

int Ssd1306Driver::width() const {
    return WIDTH;
}

int Ssd1306Driver::height() const {
    return HEIGHT;
}

}  // namespace vipi
