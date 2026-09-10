#include "touch_manager.h"

#include "esp_timer.h"

namespace {

uint32_t now_ms() {
    return static_cast<uint32_t>(
        esp_timer_get_time() / 1000ULL);
}

}  // namespace

bool ViPiTouchManager::begin(
    int vol_up_pin,
    int vol_down_pin,
    int action_pin) {

    vol_up_.pin = vol_up_pin;
    vol_down_.pin = vol_down_pin;
    action_.pin = action_pin;

    const int pins[] = {
        vol_up_pin,
        vol_down_pin,
        action_pin
    };

    for (int pin : pins) {
        if (pin < 0) {
            continue;
        }

        gpio_reset_pin(
            static_cast<gpio_num_t>(pin));

        gpio_set_direction(
            static_cast<gpio_num_t>(pin),
            GPIO_MODE_INPUT);

        gpio_set_pull_mode(
            static_cast<gpio_num_t>(pin),
            GPIO_PULLUP_ONLY);
    }

    return true;
}

bool ViPiTouchManager::pressed_(
    const Button &button) {

    if (button.pin < 0) {
        return false;
    }

    return gpio_get_level(
        static_cast<gpio_num_t>(
            button.pin)) == 0;
}

ViPiButtonEvent ViPiTouchManager::update_button_(
    Button &button,
    ViPiButtonEvent event,
    bool repeat) {

    const uint32_t now = now_ms();
    const bool is_down = pressed_(button);

    if (is_down && !button.down) {
        button.down = true;
        button.long_started = false;
        button.pressed_ms = now;
        button.last_repeat_ms = now;

        return event;
    }

    if (!is_down && button.down) {
        button.down = false;
        button.long_started = false;
        return ViPiButtonEvent::NONE;
    }

    if (is_down &&
        repeat &&
        now - button.pressed_ms >= 500 &&
        now - button.last_repeat_ms >= 120) {

        button.long_started = true;
        button.last_repeat_ms = now;
        return event;
    }

    return ViPiButtonEvent::NONE;
}

ViPiButtonEvent ViPiTouchManager::update() {
    auto event = update_button_(
        vol_up_,
        ViPiButtonEvent::VOL_UP,
        true);

    if (event != ViPiButtonEvent::NONE) {
        return event;
    }

    event = update_button_(
        vol_down_,
        ViPiButtonEvent::VOL_DOWN,
        true);

    if (event != ViPiButtonEvent::NONE) {
        return event;
    }

    return update_button_(
        action_,
        ViPiButtonEvent::ACTION,
        false);
}
