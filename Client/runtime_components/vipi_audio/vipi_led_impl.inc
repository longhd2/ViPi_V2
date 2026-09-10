void ViPiAudio::set_led_(bool on) {
  gpio_set_level((gpio_num_t) led_pin_, on ? 1 : 0);
}

