#pragma once

#include "../display/display_types.h"

namespace vipi {

struct PurpleBoardProfile {
    static DisplayConfig display() {
        DisplayConfig cfg;

        cfg.enabled = true;
        cfg.type = DisplayType::SSD1306;

        cfg.width = 128;
        cfg.height = 64;

        cfg.oled_sda = 41;
        cfg.oled_scl = 42;

        cfg.mirror_x = true;
        cfg.mirror_y = true;

        return cfg;
    }

    static constexpr int MIC_WS = 4;
    static constexpr int MIC_SCK = 5;
    static constexpr int MIC_DIN = 6;

    static constexpr int SPK_DOUT = 7;
    static constexpr int SPK_BCLK = 15;
    static constexpr int SPK_LRCK = 16;

    static constexpr int LED_PIN = 48;
    static constexpr int LED_COUNT = 1;
};

}  // namespace vipi
