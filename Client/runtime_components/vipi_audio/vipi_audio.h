#pragma once
#include "esphome/core/component.h"
#include <string>
#include <atomic>

#include "esp_http_server.h"

namespace esphome {
namespace vipi_audio {

class ViPiAudio : public Component {
 public:
  void set_server_ip(const std::string &v) { server_ip_ = v; }
  void set_command(const std::string &v) { command_ = v; }
  void set_threshold(float v) { threshold_ = v; }
  void set_wake_record_ms(uint32_t v) { wake_record_ms_ = v; }
  void set_pins(int mws,int mbclk,int mdin,int sbclk,int sws,int sdout,int ptt,int led) {
    mic_ws_=mws; mic_bclk_=mbclk; mic_din_=mdin;
    spk_bclk_=sbclk; spk_ws_=sws; spk_dout_=sdout; ptt_=ptt; led_pin_=led;
  }
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  static void audio_task_entry_(void *);
  static void tcp_task_entry_(void *);
  static void music_task_entry_(void *);
  void audio_task_();
  void tcp_task_();
  void music_task_();
  void request_music_(const std::string &url, const std::string &format);
  void stop_music_();
  void handle_buttons_();
  void change_volume_(int delta);
  void action_button_();
  bool init_i2s_();
  bool init_multinet_();
  void apply_multinet_config_();
  void send_discovery_();
  bool auto_find_server_();
  void set_led_(bool on);

  void load_settings_();
  bool save_settings_();
  void start_web_();
  void stop_web_();
  static esp_err_t web_root_(httpd_req_t *req);
  static esp_err_t web_save_(httpd_req_t *req);
  static esp_err_t web_find_(httpd_req_t *req);
  static esp_err_t web_status_(httpd_req_t *req);
  static esp_err_t web_wifi_status_(httpd_req_t *req);
  static esp_err_t web_wifi_scan_(httpd_req_t *req);
  static esp_err_t web_wifi_connect_(httpd_req_t *req);
  static esp_err_t web_logs_(httpd_req_t *req);
  static esp_err_t web_music_play_(httpd_req_t *req);
  static esp_err_t web_music_stop_(httpd_req_t *req);
  static esp_err_t web_ota_(httpd_req_t *req);
  static std::string url_decode_(const std::string &s);
  static std::string form_value_(const std::string &body, const char *key);

  // Validate GPIO assignments before saving hardware configuration.
  bool validate_gpio_config_(std::string &error) const;
  void apply_hardware_profile_();

  std::string server_ip_{"192.168.1.222"};
  std::string command_{"V B"};
  float threshold_{0.20f};
  uint32_t wake_record_ms_{6000};
  int volume_{15};
  std::string room_{"ViPi"};

  // Hardware profile:
  // 0 = Purple Board
  // 1 = Custom
  int hardware_profile_{0};

  bool discovery_sent_{false};
  uint32_t last_discovery_ms_{0};

  // Network transition state for local system sounds.
  bool wifi_state_initialized_{false};
  bool wifi_was_connected_{false};

  bool web_started_{false};
  uint32_t last_web_retry_ms_{0};
  httpd_handle_t web_server_{nullptr};
  std::atomic<bool> config_dirty_{false};
  std::atomic<bool> music_stop_{false};
  std::atomic<bool> music_pending_{false};
  std::string music_url_;
  std::string music_format_{"mp3"};
  void *music_task_handle_{nullptr};

  int mic_ws_{4}, mic_bclk_{5}, mic_din_{6};
  int spk_bclk_{15}, spk_ws_{16}, spk_dout_{7};
  int ptt_{-1};

  // ----------------------------------------------------------
  // Optional hardware configuration
  // Purple Board defaults
  // ----------------------------------------------------------

  // WS2812 LED ring.
  // No hardware detection is required.
  int led_pin_{48};
  int led_count_{24};
  int led_brightness_{50};
  bool led_reverse_{false};
  int led_offset_{0};

  // Display:
  // 0 = None
  // 1 = SSD1306 I2C
  // 2 = ST7789 SPI (reserved for later)
  int display_type_{1};
  int display_sda_{41};
  int display_scl_{42};
  int display_address_{0x3C};

  // Physical buttons.
  bool touch_enabled_{true};
  int touch_vol_up_{40};
  int touch_vol_down_{39};
  int touch_action_{0};
};
}  // namespace vipi_audio
}  // namespace esphome
