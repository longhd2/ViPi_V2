#pragma once

#include <string>

#include "../display/display_types.h"

namespace vipi {

class DisplayWebConfig {
public:
    static const char *type_name(DisplayType type) {
        switch (type) {
            case DisplayType::NONE:
                return "None";

            case DisplayType::SSD1306:
                return "OLED SSD1306 128x64 I2C";

            case DisplayType::ST7789:
                return "ST7789 SPI";
        }

        return "None";
    }

    static bool uses_i2c(DisplayType type) {
        return type == DisplayType::SSD1306;
    }

    static bool uses_spi(DisplayType type) {
        return type == DisplayType::ST7789;
    }
};

}  // namespace vipi
