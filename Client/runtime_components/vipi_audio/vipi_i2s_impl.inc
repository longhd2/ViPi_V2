bool ViPiAudio::init_i2s_() {
  i2s_chan_config_t rx_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  if (i2s_new_channel(&rx_cfg, nullptr, &rx_chan) != ESP_OK) return false;
  i2s_std_config_t mic = {};
  mic.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
  mic.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
  mic.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  mic.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  mic.gpio_cfg.bclk = (gpio_num_t) mic_bclk_;
  mic.gpio_cfg.ws = (gpio_num_t) mic_ws_;
  mic.gpio_cfg.dout = I2S_GPIO_UNUSED;
  mic.gpio_cfg.din = (gpio_num_t) mic_din_;
  if (i2s_channel_init_std_mode(rx_chan, &mic) != ESP_OK || i2s_channel_enable(rx_chan) != ESP_OK) return false;

  i2s_chan_config_t tx_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  if (i2s_new_channel(&tx_cfg, &tx_chan, nullptr) != ESP_OK) return false;
  i2s_std_config_t spk = {};
  spk.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(16000);
  spk.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  spk.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  spk.gpio_cfg.bclk = (gpio_num_t) spk_bclk_;
  spk.gpio_cfg.ws = (gpio_num_t) spk_ws_;
  spk.gpio_cfg.dout = (gpio_num_t) spk_dout_;
  spk.gpio_cfg.din = I2S_GPIO_UNUSED;
  if (i2s_channel_init_std_mode(tx_chan, &spk) != ESP_OK || i2s_channel_enable(tx_chan) != ESP_OK) return false;

  ESP_LOGI(TAG, "I2S OK MIC=%d/%d/%d SPK=%d/%d/%d", mic_ws_,mic_bclk_,mic_din_,spk_bclk_,spk_ws_,spk_dout_);
  return true;
}

