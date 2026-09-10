#pragma once

#include "display_driver.h"

namespace vipi {

class St7789Driver : public DisplayDriver {
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
    DisplayConfig config_;
    bool ready_ = false;

    // The actual ESPHome display object is bound by the platform adapter.
    // This keeps ViPi display state/rendering independent from ESPHome internals.
};

}  // namespace vipi
