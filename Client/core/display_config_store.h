#pragma once

#include "../display/display_types.h"

namespace vipi {

/*
 * NVS keys used by Display configuration.
 *
 * Namespace: vipi2
 *
 * display_type
 * display_width
 * display_height
 * display_rotation
 * display_brightness
 * display_mirror_x
 * display_mirror_y
 * display_invert
 *
 * oled_sda
 * oled_scl
 * oled_address
 *
 * display_mosi
 * display_clk
 * display_dc
 * display_rst
 * display_cs
 * display_backlight
 *
 * The actual Preferences/NVS adapter belongs in ConfigManager so the
 * Display module stays independent from storage implementation.
 */
class DisplayConfigStore {
public:
    static DisplayConfig purple_board_defaults() {
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
};

}  // namespace vipi
