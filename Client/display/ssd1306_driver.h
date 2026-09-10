#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "display_driver.h"

namespace vipi {

class Ssd1306Driver : public DisplayDriver {
public:
    bool begin(const DisplayConfig &config) override;

    void clear() override;
    void present() override;
    void set_brightness(uint8_t percent) override;

    void draw_text(
        int x,
        int y,
        const std::string &text,
        uint8_t size,
        bool centered
    ) override;

    void draw_icon(
        int x,
        int y,
        const uint8_t *bitmap,
        int width,
        int height
    ) override;

    void draw_progress(
        int x,
        int y,
        int width,
        int height,
        float ratio
    ) override;

    int width() const override;
    int height() const override;

private:
    static constexpr int WIDTH = 128;
    static constexpr int HEIGHT = 64;
    static constexpr int BUFFER_SIZE = WIDTH * HEIGHT / 8;

    DisplayConfig config_;
    std::array<uint8_t, BUFFER_SIZE> buffer_{};
    bool ready_ = false;

    bool write_command_(uint8_t command);
    bool write_data_(const uint8_t *data, size_t length);

    void draw_pixel_(int x, int y, bool on = true);
    void draw_char_(int x, int y, char c, uint8_t size);
    int text_width_(const std::string &text, uint8_t size) const;
};

}  // namespace vipi
