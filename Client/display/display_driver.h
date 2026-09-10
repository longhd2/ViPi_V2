#pragma once

#include <cstdint>
#include <string>

#include "display_types.h"

namespace vipi {

class DisplayDriver {
public:
    virtual ~DisplayDriver() = default;

    virtual bool begin(const DisplayConfig &config) = 0;
    virtual void clear() = 0;
    virtual void present() = 0;

    virtual void set_brightness(uint8_t percent) = 0;

    virtual void draw_text(
        int x,
        int y,
        const std::string &text,
        uint8_t size = 1,
        bool centered = false
    ) = 0;

    virtual void draw_icon(
        int x,
        int y,
        const uint8_t *bitmap,
        int width,
        int height
    ) = 0;

    virtual void draw_progress(
        int x,
        int y,
        int width,
        int height,
        float ratio
    ) = 0;

    virtual int width() const = 0;
    virtual int height() const = 0;
};

}  // namespace vipi
