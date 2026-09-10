#pragma once
#include "vipi_state.h"
#include "driver/gpio.h"
namespace esphome { namespace vipi_audio {
class ViPiLed { public: void setup(int pin); void set_state(ViPiState state); void tick(); private: int pin_{-1}; ViPiState state_{ViPiState::BOOTING}; uint32_t last_{0}; bool level_{false}; };
}}