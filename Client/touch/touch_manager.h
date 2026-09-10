#pragma once

#include <cstdint>
#include "driver/gpio.h"

enum class ViPiButtonEvent : uint8_t {
    NONE,
    VOL_UP,
    VOL_DOWN,
    ACTION
};

class ViPiTouchManager {
 public:
    bool begin(
        int vol_up_pin,
        int vol_down_pin,
        int action_pin);

    ViPiButtonEvent update();

 private:
    struct Button {
        int pin{-1};
        bool down{false};
        bool long_started{false};
        uint32_t pressed_ms{0};
        uint32_t last_repeat_ms{0};
    };

    Button vol_up_;
    Button vol_down_;
    Button action_;

    static bool pressed_(const Button &button);

    ViPiButtonEvent update_button_(
        Button &button,
        ViPiButtonEvent event,
        bool repeat);
};
