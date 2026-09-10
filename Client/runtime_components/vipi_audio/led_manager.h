#pragma once

#include <stdint.h>
#include <vector>

#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"

enum class ViPiLedState : uint8_t {
  BOOTING,
  CONNECTING,
  IDLE,
  WAKE,
  LISTENING,
  THINKING,
  SPEAKING,
  MUSIC,
  VOLUME,
  MUTED,
  TIMER,
  ALARM,
  SUCCESS,
  WARNING,
  ERROR,
  OTA
};

enum class ViPiLedEffect : uint8_t {
  NONE,
  ROTATING_BLOB,
  THINKING,
  MUTED_SILENT,
  VOLUME_DISPLAY,
  ACTION_BUTTON,
  TWINKLE,
  ERROR_EFFECT,
  SUCCESS_EFFECT,
  WARNING_EFFECT,
  FAST_PULSE,
  TIMER_RING,
  TIMER_TICK,
  RAINBOW,
  TICK,
  FLASHING_XMOS,
  FACTORY_RESET_COMING_UP,
  JACK_PLUGGED,
  JACK_UNPLUGGED
};

struct ViPiLedConfig {
  int pin{48};
  int count{24};

  // GRB = WS2812 default.
  float brightness{0.50f};

  bool reverse{false};
  int offset{0};

  uint32_t update_ms{25};
};

class ViPiLedManager {
 public:
  ViPiLedManager();

  bool begin(const ViPiLedConfig &config);

  void set_state(ViPiLedState state);
  void set_effect(ViPiLedEffect effect);

  void set_volume(float ratio);

  void set_timer(
      uint32_t seconds_left,
      uint32_t total_seconds);

  void update();
  void clear();

  ViPiLedState state() const {
    return state_;
  }

  int led_count() const {
    return config_.count;
  }

 private:
  struct Pixel {
    uint8_t r{0};
    uint8_t g{0};
    uint8_t b{0};
  };

  ViPiLedConfig config_{};

  ViPiLedState state_{ViPiLedState::BOOTING};
  ViPiLedEffect effect_{ViPiLedEffect::NONE};

  std::vector<Pixel> pixels_;
  std::vector<uint8_t> tx_buffer_;

  rmt_channel_handle_t rmt_channel_{nullptr};
  rmt_encoder_handle_t rmt_encoder_{nullptr};

  uint32_t last_update_ms_{0};
  uint32_t effect_started_ms_{0};

  float volume_ratio_{0.0f};

  uint32_t timer_left_{0};
  uint32_t timer_total_{0};

  int wrap(int index) const;
  int physical_index(int logical) const;
  int percent_to_led(float ratio) const;
  int opposite(int index) const;

  void choose_effect();

  void render(uint32_t now);

  void render_rotating_blob(uint32_t now);
  void render_thinking(uint32_t now);
  void render_muted(uint32_t now);
  void render_volume(uint32_t now);
  void render_action(uint32_t now);
  void render_twinkle(uint32_t now);

  void render_error(uint32_t now);
  void render_success(uint32_t now);
  void render_warning(uint32_t now);
  void render_fast_pulse(uint32_t now);

  void render_timer_ring(uint32_t now);
  void render_timer_tick(uint32_t now);

  void render_rainbow(uint32_t now);
  void render_tick(uint32_t now);

  void render_flashing_xmos(uint32_t now);
  void render_factory_reset(uint32_t now);
  void render_jack_plugged(uint32_t now);
  void render_jack_unplugged(uint32_t now);

  void fill(
      uint8_t r,
      uint8_t g,
      uint8_t b);

  void set_pixel(
      int index,
      uint8_t r,
      uint8_t g,
      uint8_t b);

  void fade_all(float factor);

  void show();

  Pixel wheel(uint8_t position) const;

  float pulse(
      uint32_t now,
      uint32_t period_ms) const;
};
