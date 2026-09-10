#pragma once
#include "../core/types.h"
#include "led_effects.h"
namespace vipi {
    inline LedEffect effect_for_state(DeviceState s) {
        switch(s) {
            case DeviceState::WAKE:case DeviceState::LISTENING:case DeviceState::SPEAKING:return LedEffect::ROTATING_BLOB;
            case DeviceState::THINKING:return LedEffect::THINKING;
            case DeviceState::MUTED:return LedEffect::MUTED_SILENT;
            case DeviceState::TIMER:return LedEffect::TIMER_RING;
            case DeviceState::WARNING:return LedEffect::WARNING;
            case DeviceState::ERROR:return LedEffect::ERROR;
            default:return LedEffect::OFF;
        }
    }
}
