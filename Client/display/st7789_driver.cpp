#include "st7789_driver.h"

#include <algorithm>

namespace vipi {

bool St7789Driver::begin(const DisplayConfig &config) {
    config_ = config;

    if (config_.spi_mosi < 0 ||
        config_.spi_clk < 0 ||
        config_.spi_dc < 0) {
        ready_ = false;
        return false;
    }

    ready_ = true;
    return true;
}

void St7789Driver::clear() {
    if (!ready_) {
        return;
    }
}

void St7789Driver::present() {
    if (!ready_) {
        return;
    }
}

void St7789Driver::set_brightness(uint8_t percent) {
    config_.brightness = std::min<uint8_t>(percent, 100);
}

void St7789Driver::draw_text(
    int x,
    int y,
    const std::string &text,
    uint8_t size,
    bool centered
) {
    if (!ready_) {
        return;
    }

    (void)x;
    (void)y;
    (void)text;
    (void)size;
    (void)centered;
}

void St7789Driver::draw_icon(
    int x,
    int y,
    const uint8_t *bitmap,
    int width,
    int height
) {
    if (!ready_) {
        return;
    }

    (void)x;
    (void)y;
    (void)bitmap;
    (void)width;
    (void)height;
}

void St7789Driver::draw_progress(
    int x,
    int y,
    int width,
    int height,
    float ratio
) {
    if (!ready_) {
        return;
    }

    ratio = std::clamp(ratio, 0.0f, 1.0f);

    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)ratio;
}

int St7789Driver::width() const {
    return config_.width;
}

int St7789Driver::height() const {
    return config_.height;
}

}  // namespace vipi
