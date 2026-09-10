void ViPiAudio::loop() {
  const uint32_t now = millis();

  if (!network::is_connected()) {
    discovery_sent_ = false;
    if (web_started_) stop_web_();
    return;
  }

  if (!web_started_) start_web_();

  if (config_dirty_.exchange(false)) {
    apply_multinet_config_();
    discovery_sent_ = false;
  }

  if (!discovery_sent_ || (now - last_discovery_ms_ >= 30000UL)) {
    send_discovery_();
  }
}

void ViPiAudio::setup() {
  load_settings_();
  gpio_reset_pin((gpio_num_t) led_pin_);
  gpio_set_direction((gpio_num_t) led_pin_, GPIO_MODE_OUTPUT);
  set_led_(false);
  if (ptt_ >= 0) {
    gpio_reset_pin((gpio_num_t) ptt_);
    gpio_set_direction((gpio_num_t) ptt_, GPIO_MODE_INPUT);
    gpio_pullup_en((gpio_num_t) ptt_);
  }

  if (!init_i2s_() || !init_multinet_()) { mark_failed(); return; }
  xTaskCreatePinnedToCore(audio_task_entry_, "vipi_audio", 12288, this, 5, nullptr, 1);
  xTaskCreatePinnedToCore(tcp_task_entry_, "vipi_tcp", 16384, this, 4, nullptr, 0);
  xTaskCreatePinnedToCore(music_task_entry_, "vipi_music", 16384, this, 4, (TaskHandle_t*)&music_task_handle_, 0);
}

void ViPiAudio::audio_task_entry_(void *p) { static_cast<ViPiAudio*>(p)->audio_task_(); vTaskDelete(nullptr); }
void ViPiAudio::tcp_task_entry_(void *p) { static_cast<ViPiAudio*>(p)->tcp_task_(); vTaskDelete(nullptr); }
void ViPiAudio::music_task_entry_(void *p) { static_cast<ViPiAudio*>(p)->music_task_(); vTaskDelete(nullptr); }

