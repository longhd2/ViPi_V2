#pragma once

#include <cstdint>

namespace vipi {

// Compact 5x7 ASCII font, characters 0x20..0x7E.
// Each glyph is five vertical columns, least-significant bit at the top.
extern const uint8_t FONT_5X7[95][5];

}  // namespace vipi
