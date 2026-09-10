#pragma once

#include <string>

#include "display_types.h"

namespace vipi {

struct DisplayView {
    DisplayState state = DisplayState::IDLE;

    std::string title;
    std::string primary_text;
    std::string secondary_text;

    float volume_ratio = 0.5f;
    float timer_ratio = 0.0f;

    int wifi_rssi = -100;
    bool muted = false;
};

}  // namespace vipi
