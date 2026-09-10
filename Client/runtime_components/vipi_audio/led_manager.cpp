#include "led_manager.h"

#include <algorithm>
#include <cmath>

#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "vipi_led";

static uint32_t now_ms() {
  return static_cast<uint32_t>(
      esp_timer_get_time() / 1000ULL);
}

ViPiLedManager::ViPiLedManager() = default;

bool ViPiLedManager::begin(
    const ViPiLedConfig &config) {
  config_ = config;

  if (config_.count < 1) {
    config_.count = 1;
  }

  if (config_.brightness < 0.0f) {
    config_.brightness = 0.0f;
  }

  if (config_.brightness > 1.0f) {
    config_.brightness = 1.0f;
  }

  pixels_.resize(config_.count);
  tx_buffer_.resize(config_.count * 3);

  rmt_tx_channel_config_t channel_config{};
  channel_config.gpio_num =
      static_cast<gpio_num_t>(config_.pin);
  channel_config.clk_src =
      RMT_CLK_SRC_DEFAULT;
  channel_config.resolution_hz =
      10000000;
  channel_config.mem_block_symbols =
      64;
  channel_config.trans_queue_depth =
      4;

  esp_err_t err =
      rmt_new_tx_channel(
          &channel_config,
          &rmt_channel_);

  if (err != ESP_OK) {
    ESP_LOGE(
        TAG,
        "RMT channel failed: %s",
        esp_err_to_name(err));

    return false;
  }

  rmt_bytes_encoder_config_t encoder_config{};

  // 10 MHz => 1 tick = 100 ns.
  //
  // WS2812:
  // 0: high ~0.3 us, low ~0.9 us
  // 1: high ~0.9 us, low ~0.3 us
  encoder_config.bit0.duration0 = 3;
  encoder_config.bit0.level0 = 1;
  encoder_config.bit0.duration1 = 9;
  encoder_config.bit0.level1 = 0;

  encoder_config.bit1.duration0 = 9;
  encoder_config.bit1.level0 = 1;
  encoder_config.bit1.duration1 = 3;
  encoder_config.bit1.level1 = 0;

  encoder_config.flags.msb_first = 1;

  err =
      rmt_new_bytes_encoder(
          &encoder_config,
          &rmt_encoder_);

  if (err != ESP_OK) {
    ESP_LOGE(
        TAG,
        "RMT encoder failed: %s",
        esp_err_to_name(err));

    return false;
  }

  err = rmt_enable(rmt_channel_);

  if (err != ESP_OK) {
    ESP_LOGE(
        TAG,
        "RMT enable failed: %s",
        esp_err_to_name(err));

    return false;
  }

  clear();

  state_ = ViPiLedState::BOOTING;
  effect_started_ms_ = now_ms();

  choose_effect();

  ESP_LOGI(
      TAG,
      "WS2812 ready GPIO%d count=%d",
      config_.pin,
      config_.count);

  return true;
}

int ViPiLedManager::wrap(int index) const {
  int n = config_.count;

  if (n <= 0) {
    return 0;
  }

  index %= n;

  if (index < 0) {
    index += n;
  }

  return index;
}

int ViPiLedManager::physical_index(
    int logical) const {
  int index =
      wrap(logical + config_.offset);

  if (config_.reverse) {
    index =
        wrap(config_.count - 1 - index);
  }

  return index;
}

int ViPiLedManager::percent_to_led(
    float ratio) const {
  ratio =
      std::max(
          0.0f,
          std::min(1.0f, ratio));

  return static_cast<int>(
      std::lround(
          ratio * config_.count));
}

int ViPiLedManager::opposite(
    int index) const {
  return wrap(
      index + config_.count / 2);
}

void ViPiLedManager::set_state(
    ViPiLedState state) {
  if (state_ == state) {
    return;
  }

  state_ = state;
  effect_started_ms_ = now_ms();

  choose_effect();
}

void ViPiLedManager::set_effect(
    ViPiLedEffect effect) {
  effect_ = effect;
  effect_started_ms_ = now_ms();
}

void ViPiLedManager::set_volume(
    float ratio) {
  volume_ratio_ =
      std::max(
          0.0f,
          std::min(1.0f, ratio));

  set_effect(
      ViPiLedEffect::VOLUME_DISPLAY);
}

void ViPiLedManager::set_timer(
    uint32_t seconds_left,
    uint32_t total_seconds) {
  timer_left_ = seconds_left;
  timer_total_ = total_seconds;

  set_effect(
      ViPiLedEffect::TIMER_RING);
}

void ViPiLedManager::choose_effect() {
  switch (state_) {
    case ViPiLedState::BOOTING:
      effect_ = ViPiLedEffect::FAST_PULSE;
      break;

    case ViPiLedState::CONNECTING:
      effect_ = ViPiLedEffect::TWINKLE;
      break;

    case ViPiLedState::IDLE:
      effect_ = ViPiLedEffect::NONE;
      break;

    case ViPiLedState::WAKE:
    case ViPiLedState::LISTENING:
    case ViPiLedState::SPEAKING:
      effect_ =
          ViPiLedEffect::ROTATING_BLOB;
      break;

    case ViPiLedState::THINKING:
      effect_ =
          ViPiLedEffect::THINKING;
      break;

    case ViPiLedState::MUSIC:
      effect_ =
          ViPiLedEffect::RAINBOW;
      break;

    case ViPiLedState::VOLUME:
      effect_ =
          ViPiLedEffect::VOLUME_DISPLAY;
      break;

    case ViPiLedState::MUTED:
      effect_ =
          ViPiLedEffect::MUTED_SILENT;
      break;

    case ViPiLedState::TIMER:
      effect_ =
          ViPiLedEffect::TIMER_TICK;
      break;

    case ViPiLedState::ALARM:
      effect_ =
          ViPiLedEffect::TIMER_RING;
      break;

    case ViPiLedState::SUCCESS:
      effect_ =
          ViPiLedEffect::SUCCESS_EFFECT;
      break;

    case ViPiLedState::WARNING:
      effect_ =
          ViPiLedEffect::WARNING_EFFECT;
      break;

    case ViPiLedState::ERROR:
      effect_ =
          ViPiLedEffect::ERROR_EFFECT;
      break;

    case ViPiLedState::OTA:
      effect_ =
          ViPiLedEffect::FAST_PULSE;
      break;
  }
}

void ViPiLedManager::fill(
    uint8_t r,
    uint8_t g,
    uint8_t b) {
  for (auto &pixel : pixels_) {
    pixel.r = r;
    pixel.g = g;
    pixel.b = b;
  }
}

void ViPiLedManager::set_pixel(
    int index,
    uint8_t r,
    uint8_t g,
    uint8_t b) {
  if (pixels_.empty()) {
    return;
  }

  int p = physical_index(index);

  pixels_[p].r = r;
  pixels_[p].g = g;
  pixels_[p].b = b;
}

void ViPiLedManager::fade_all(
    float factor) {
  for (auto &pixel : pixels_) {
    pixel.r =
        static_cast<uint8_t>(
            pixel.r * factor);

    pixel.g =
        static_cast<uint8_t>(
            pixel.g * factor);

    pixel.b =
        static_cast<uint8_t>(
            pixel.b * factor);
  }
}

float ViPiLedManager::pulse(
    uint32_t now,
    uint32_t period_ms) const {
  if (period_ms == 0) {
    return 1.0f;
  }

  float phase =
      static_cast<float>(
          (now - effect_started_ms_) %
          period_ms) /
      static_cast<float>(period_ms);

  return
      0.5f -
      0.5f *
          std::cos(
              phase *
              2.0f *
              3.14159265f);
}

ViPiLedManager::Pixel
ViPiLedManager::wheel(
    uint8_t position) const {
  Pixel pixel{};

  position =
      255 - position;

  if (position < 85) {
    pixel.r =
        255 - position * 3;
    pixel.g = 0;
    pixel.b =
        position * 3;

    return pixel;
  }

  if (position < 170) {
    position -= 85;

    pixel.r = 0;
    pixel.g =
        position * 3;
    pixel.b =
        255 - position * 3;

    return pixel;
  }

  position -= 170;

  pixel.r =
      position * 3;
  pixel.g =
      255 - position * 3;
  pixel.b = 0;

  return pixel;
}

void ViPiLedManager::render_rotating_blob(
    uint32_t now) {
  fill(0, 0, 0);

  int n = config_.count;

  uint32_t speed =
      state_ == ViPiLedState::LISTENING
          ? 55
          : 80;

  int head =
      static_cast<int>(
          (now - effect_started_ms_) /
          speed);

  // Reply/SPEAKING rotates opposite direction.
  if (state_ == ViPiLedState::SPEAKING) {
    head = -head;
  }

  head = wrap(head);

  int tail =
      std::max(
          2,
          n / 6);

  for (int i = 0; i < tail; ++i) {
    float strength =
        1.0f -
        static_cast<float>(i) /
            static_cast<float>(tail);

    uint8_t level =
        static_cast<uint8_t>(
            220.0f * strength);

    if (state_ == ViPiLedState::LISTENING) {
      set_pixel(
          head - i,
          0,
          level,
          level);
    } else if (
        state_ ==
        ViPiLedState::SPEAKING) {
      set_pixel(
          head + i,
          0,
          level,
          40);
    } else {
      set_pixel(
          head - i,
          0,
          80,
          level);
    }
  }

  // Opposite small marker.
  set_pixel(
      opposite(head),
      8,
      8,
      20);
}

void ViPiLedManager::render_thinking(
    uint32_t now) {
  fill(0, 0, 0);

  int n = config_.count;

  int quarter =
      std::max(1, n / 4);

  int step =
      static_cast<int>(
          (now - effect_started_ms_) /
          100);

  for (int q = 0; q < 4; ++q) {
    int pos =
        wrap(
            step +
            q * quarter);

    set_pixel(
        pos,
        90,
        0,
        180);

    set_pixel(
        pos - 1,
        25,
        0,
        60);
  }
}

void ViPiLedManager::render_muted(
    uint32_t now) {
  float value =
      0.20f +
      pulse(now, 1800) *
          0.35f;

  uint8_t level =
      static_cast<uint8_t>(
          180.0f * value);

  fill(level, 0, 0);
}

void ViPiLedManager::render_volume(
    uint32_t) {
  fill(0, 0, 0);

  int lit =
      percent_to_led(
          volume_ratio_);

  for (int i = 0; i < lit; ++i) {
    set_pixel(
        i,
        0,
        130,
        220);
  }
}

void ViPiLedManager::render_action(
    uint32_t now) {
  fill(0, 0, 0);

  bool on =
      ((now - effect_started_ms_) /
       100) %
          2 ==
      0;

  if (on) {
    fill(180, 180, 180);
  }
}

void ViPiLedManager::render_twinkle(
    uint32_t now) {
  fill(0, 0, 0);

  int n = config_.count;

  uint32_t step =
      (now - effect_started_ms_) /
      120;

  int a =
      wrap(
          static_cast<int>(
              step * 7 + 3));

  int b =
      wrap(
          static_cast<int>(
              step * 11 + n / 3));

  int c =
      wrap(
          static_cast<int>(
              step * 5 + n / 2));

  set_pixel(a, 25, 25, 90);
  set_pixel(b, 15, 60, 80);
  set_pixel(c, 40, 40, 100);
}

void ViPiLedManager::render_error(
    uint32_t now) {
  bool on =
      ((now - effect_started_ms_) /
       150) %
          2 ==
      0;

  fill(
      on ? 220 : 0,
      0,
      0);
}

void ViPiLedManager::render_success(
    uint32_t now) {
  float value =
      1.0f -
      std::min(
          1.0f,
          static_cast<float>(
              now - effect_started_ms_) /
              900.0f);

  fill(
      0,
      static_cast<uint8_t>(
          220 * value),
      0);
}

void ViPiLedManager::render_warning(
    uint32_t now) {
  bool on =
      ((now - effect_started_ms_) %
       1000) <
      300;

  fill(
      on ? 220 : 30,
      on ? 90 : 10,
      0);
}

void ViPiLedManager::render_fast_pulse(
    uint32_t now) {
  float value =
      pulse(now, 500);

  uint8_t level =
      static_cast<uint8_t>(
          180 * value);

  fill(
      0,
      level / 2,
      level);
}

void ViPiLedManager::render_timer_ring(
    uint32_t now) {
  fill(0, 0, 0);

  if (timer_total_ == 0) {
    bool on =
        ((now - effect_started_ms_) /
         200) %
            2 ==
        0;

    if (on) {
      fill(220, 60, 0);
    }

    return;
  }

  float ratio =
      static_cast<float>(
          timer_left_) /
      static_cast<float>(
          timer_total_);

  int lit =
      percent_to_led(ratio);

  for (int i = 0; i < lit; ++i) {
    set_pixel(
        i,
        220,
        90,
        0);
  }
}

void ViPiLedManager::render_timer_tick(
    uint32_t now) {
  fill(0, 0, 0);

  int pos =
      wrap(
          static_cast<int>(
              (now -
               effect_started_ms_) /
              1000));

  set_pixel(
      pos,
      220,
      90,
      0);
}

void ViPiLedManager::render_rainbow(
    uint32_t now) {
  int n = config_.count;

  uint8_t shift =
      static_cast<uint8_t>(
          (now -
           effect_started_ms_) /
          20);

  for (int i = 0; i < n; ++i) {
    uint8_t position =
        static_cast<uint8_t>(
            (i * 256 / n +
             shift) &
            0xFF);

    Pixel p = wheel(position);

    set_pixel(
        i,
        p.r,
        p.g,
        p.b);
  }
}

void ViPiLedManager::render_tick(
    uint32_t now) {
  fill(0, 0, 0);

  if (
      now - effect_started_ms_ <
      220) {
    fill(0, 180, 80);
  }
}

void ViPiLedManager::render_flashing_xmos(
    uint32_t now) {
  bool on =
      ((now - effect_started_ms_) /
       80) %
          2 ==
      0;

  fill(
      on ? 150 : 0,
      on ? 150 : 0,
      on ? 150 : 0);
}

void ViPiLedManager::render_factory_reset(
    uint32_t now) {
  fill(0, 0, 0);

  int head =
      wrap(
          static_cast<int>(
              (now -
               effect_started_ms_) /
              45));

  int width =
      std::max(
          2,
          config_.count / 5);

  for (int i = 0; i < width; ++i) {
    set_pixel(
        head - i,
        220,
        0,
        0);
  }
}

void ViPiLedManager::render_jack_plugged(
    uint32_t now) {
  float value =
      1.0f -
      std::min(
          1.0f,
          static_cast<float>(
              now - effect_started_ms_) /
              700.0f);

  fill(
      0,
      static_cast<uint8_t>(
          180 * value),
      static_cast<uint8_t>(
          100 * value));
}

void ViPiLedManager::render_jack_unplugged(
    uint32_t now) {
  float value =
      1.0f -
      std::min(
          1.0f,
          static_cast<float>(
              now - effect_started_ms_) /
              700.0f);

  fill(
      static_cast<uint8_t>(
          180 * value),
      static_cast<uint8_t>(
          40 * value),
      0);
}

void ViPiLedManager::render(
    uint32_t now) {
  switch (effect_) {
    case ViPiLedEffect::NONE:
      fill(0, 0, 0);
      break;

    case ViPiLedEffect::ROTATING_BLOB:
      render_rotating_blob(now);
      break;

    case ViPiLedEffect::THINKING:
      render_thinking(now);
      break;

    case ViPiLedEffect::MUTED_SILENT:
      render_muted(now);
      break;

    case ViPiLedEffect::VOLUME_DISPLAY:
      render_volume(now);
      break;

    case ViPiLedEffect::ACTION_BUTTON:
      render_action(now);
      break;

    case ViPiLedEffect::TWINKLE:
      render_twinkle(now);
      break;

    case ViPiLedEffect::ERROR_EFFECT:
      render_error(now);
      break;

    case ViPiLedEffect::SUCCESS_EFFECT:
      render_success(now);
      break;

    case ViPiLedEffect::WARNING_EFFECT:
      render_warning(now);
      break;

    case ViPiLedEffect::FAST_PULSE:
      render_fast_pulse(now);
      break;

    case ViPiLedEffect::TIMER_RING:
      render_timer_ring(now);
      break;

    case ViPiLedEffect::TIMER_TICK:
      render_timer_tick(now);
      break;

    case ViPiLedEffect::RAINBOW:
      render_rainbow(now);
      break;

    case ViPiLedEffect::TICK:
      render_tick(now);
      break;

    case ViPiLedEffect::FLASHING_XMOS:
      render_flashing_xmos(now);
      break;

    case ViPiLedEffect::FACTORY_RESET_COMING_UP:
      render_factory_reset(now);
      break;

    case ViPiLedEffect::JACK_PLUGGED:
      render_jack_plugged(now);
      break;

    case ViPiLedEffect::JACK_UNPLUGGED:
      render_jack_unplugged(now);
      break;
  }
}

void ViPiLedManager::show() {
  if (
      rmt_channel_ == nullptr ||
      rmt_encoder_ == nullptr) {
    return;
  }

  for (int i = 0; i < config_.count; ++i) {
    const Pixel &p = pixels_[i];

    uint8_t r =
        static_cast<uint8_t>(
            p.r * config_.brightness);

    uint8_t g =
        static_cast<uint8_t>(
            p.g * config_.brightness);

    uint8_t b =
        static_cast<uint8_t>(
            p.b * config_.brightness);

    // WS2812 byte order = GRB.
    tx_buffer_[i * 3 + 0] = g;
    tx_buffer_[i * 3 + 1] = r;
    tx_buffer_[i * 3 + 2] = b;
  }

  rmt_transmit_config_t tx_config{};
  tx_config.loop_count = 0;

  esp_err_t err =
      rmt_transmit(
          rmt_channel_,
          rmt_encoder_,
          tx_buffer_.data(),
          tx_buffer_.size(),
          &tx_config);

  if (err == ESP_OK) {
    rmt_tx_wait_all_done(
        rmt_channel_,
        pdMS_TO_TICKS(20));
  }
}

void ViPiLedManager::update() {
  uint32_t now = now_ms();

  if (
      last_update_ms_ != 0 &&
      now - last_update_ms_ <
          config_.update_ms) {
    return;
  }

  last_update_ms_ = now;

  render(now);
  show();
}

void ViPiLedManager::clear() {
  fill(0, 0, 0);
  show();
}
