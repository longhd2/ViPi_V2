#include "vipi_audio.h"
#include "vipi_diagnostics.h"
#include "display_manager.h"
#include "touch_manager.h"
#include "esphome/core/hal.h"
#include "esphome/components/network/util.h"
#include "esphome/core/log.h"
#include "esphome/components/wifi/wifi_component.h"

#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_mn_iface.h"
#include "esp_mn_models.h"
#include "esp_mn_speech_commands.h"
#include "model_path.h"
#include "opus.h"

#include "lwip/sockets.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_ota_ops.h"
#include "esp_system.h"
#include "esp_audio_simple_dec.h"
#include "esp_audio_simple_dec_default.h"
#include "esp_audio_dec_default.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include "simple_dec/impl/esp_m4a_parse.h"
#include "audio_manager.h"
#include "led_manager.h"
#include "sound_manager.h"



extern "C" void *media_lib_module_malloc(size_t size) {
  void *caller = __builtin_return_address(0);
  ESP_LOGE("VIPI.ALLOC", "MALLOC ENTER size=%u caller=%p",
           (unsigned) size, caller);
  void *p = malloc(size);
  ESP_LOGW("media_alloc",
           "malloc size=%u -> %p internal=%u largest_internal=%u psram=%u largest_psram=%u",
           (unsigned) size,
           p,
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  return p;
}

extern "C" void *media_lib_module_calloc(size_t n, size_t size) {
  void *caller = __builtin_return_address(0);
  ESP_LOGE("VIPI.ALLOC", "CALLOC ENTER n=%u size=%u total=%u caller=%p",
           (unsigned) n,
           (unsigned) size,
           (unsigned) (n * size),
           caller);
  void *p = calloc(n, size);
  ESP_LOGW("media_alloc",
           "calloc n=%u size=%u total=%u -> %p internal=%u largest_internal=%u psram=%u largest_psram=%u",
           (unsigned) n,
           (unsigned) size,
           (unsigned) (n * size),
           p,
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  return p;
}

extern "C" void *media_lib_module_realloc(void *ptr, size_t size) {
  void *caller = __builtin_return_address(0);
  ESP_LOGE("VIPI.ALLOC", "REALLOC ENTER ptr=%p size=%u caller=%p",
           ptr, (unsigned) size, caller);
  void *p = realloc(ptr, size);
  ESP_LOGW("media_alloc",
           "realloc old=%p size=%u -> %p internal=%u largest_internal=%u psram=%u largest_psram=%u",
           ptr,
           (unsigned) size,
           p,
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  return p;
}

namespace esphome {
namespace vipi_audio {

static const char *TAG = "vipi_audio";
static i2s_chan_handle_t rx_chan = nullptr;
static i2s_chan_handle_t tx_chan = nullptr;

static srmodel_list_t *models = nullptr;
static esp_mn_iface_t *mn = nullptr;
static model_iface_data_t *mn_data = nullptr;

static volatile bool playing = false;
static volatile bool wake_stream = false;
static volatile uint32_t wake_until = 0;

struct ViPiMediaResumeContext {
  bool valid{false};
  bool interrupted_by_assistant{false};

  std::string url;
  std::string format;

  uint32_t started_ms{0};
  uint32_t position_ms{0};
};

static ViPiMediaResumeContext media_resume_ctx;

static volatile bool media_resume_after_tts = false;
static volatile bool media_user_stop = false;

static void clear_media_resume_context() {
  media_resume_ctx.valid = false;
  media_resume_ctx.interrupted_by_assistant = false;
  media_resume_ctx.url.clear();
  media_resume_ctx.format.clear();
  media_resume_ctx.started_ms = 0;
  media_resume_ctx.position_ms = 0;

  media_resume_after_tts = false;
}



static ViPiAudioManager vipi_audio_manager;
static ViPiLedManager vipi_led_manager;
static ViPiTouchManager vipi_touch_manager;

// SoundManager hien tai da duoc build PASS.
// Ta khoi tao no sau khi I2S TX san sang.
static ViPiSoundManager *vipi_sound_manager = nullptr;



static uint8_t linear_to_ulaw(int16_t pcm) {
  const int BIAS=0x84, CLIP=32635;
  int mask, seg;
  uint8_t uval;
  if (pcm < 0) { pcm = -pcm; mask = 0x7F; } else mask = 0xFF;
  if (pcm > CLIP) pcm = CLIP;
  pcm += BIAS;
  if (pcm <= 0xFF) seg=0; else if (pcm<=0x1FF) seg=1; else if (pcm<=0x3FF) seg=2;
  else if (pcm<=0x7FF) seg=3; else if (pcm<=0xFFF) seg=4; else if (pcm<=0x1FFF) seg=5;
  else if (pcm<=0x3FFF) seg=6; else seg=7;
  uval = (seg << 4) | ((pcm >> (seg + 3)) & 0xF);
  return uval ^ mask;
}


// ViPi V2 modular implementation

// ===== BEGIN vipi_led_impl.inc =====
void ViPiAudio::set_led_(bool on) {
  // Compatibility bridge for old V1 calls.
  // New code should use ViPiLedState directly.
  vipi_led_manager.set_state(
      on
          ? ViPiLedState::LISTENING
          : ViPiLedState::IDLE);
}


// ===== END vipi_led_impl.inc =====


// ===== BEGIN vipi_i2s_impl.inc =====
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


// ===== END vipi_i2s_impl.inc =====


// ===== BEGIN vipi_wake_impl.inc =====
bool ViPiAudio::init_multinet_() {
  models = esp_srmodel_init("model");
  if (!models || models->num <= 0) { ESP_LOGE(TAG, "No model partition"); return false; }
  char *name = esp_srmodel_filter(models, ESP_MN_PREFIX, "en");
  if (!name) name = esp_srmodel_filter(models, ESP_MN_PREFIX, nullptr);
  if (!name) { ESP_LOGE(TAG, "MultiNet English model missing"); return false; }
  mn = esp_mn_handle_from_name(name);
  if (!mn) return false;
  mn_data = mn->create(name, 6000);
  if (!mn_data) return false;
  mn->set_det_threshold(mn_data, threshold_);
  esp_mn_commands_clear();
  esp_mn_commands_add(1, command_.c_str());
  esp_mn_commands_update();
  ESP_LOGI(TAG, "MultiNet ready: %s / '%s' threshold=%.2f", name, command_.c_str(), threshold_);
  return true;
}


// ===== END vipi_wake_impl.inc =====


// ===== BEGIN vipi_discovery_impl.inc =====
void ViPiAudio::send_discovery_() {
  if (!network::is_connected()) return;

  int s = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
  if (s < 0) {
    ESP_LOGW(TAG, "SERVER HANDSHAKE: socket failed");
    return;
  }

  sockaddr_in dst{};
  dst.sin_family = AF_INET;
  dst.sin_port = htons(12345);
  if (inet_pton(AF_INET, server_ip_.c_str(), &dst.sin_addr) != 1) {
    ESP_LOGW(TAG, "SERVER HANDSHAKE: bad server IP %s", server_ip_.c_str());
    close(s);
    return;
  }

  const char *msg = "vipi-v2 PTT";
  int sent = sendto(s, msg, strlen(msg), 0, (sockaddr *) &dst, sizeof(dst));
  close(s);

  if (sent == (int) strlen(msg)) {
    discovery_sent_ = true;
    last_discovery_ms_ = millis();
    ESP_LOGI(TAG, "SERVER HANDSHAKE SENT -> %s:12345 [%s]",
             server_ip_.c_str(), msg);
  } else {
    ESP_LOGW(TAG, "SERVER HANDSHAKE SEND FAILED -> %s:12345", server_ip_.c_str());
  }
}


// ===== END vipi_discovery_impl.inc =====


// ===== BEGIN vipi_lifecycle_impl.inc =====
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



static void vipi_init_sound_manager() {
  if (vipi_sound_manager == nullptr) {
    vipi_sound_manager = new ViPiSoundManager();
    if (vipi_sound_manager != nullptr) {
      vipi_sound_manager->begin(tx_chan);
    }
  }
}

static void vipi_play_wake_sound(int volume) {
  if (vipi_sound_manager == nullptr) {
    return;
  }

  if (!vipi_audio_manager.acquire(
          ViPiAudioManager::Owner::SYSTEM,
          pdMS_TO_TICKS(2000))) {
    ESP_LOGW(TAG, "WAKE sound: speaker busy");
    return;
  }

  ESP_LOGI(TAG, "WAKE sound: wake_word_triggered.wav");

  vipi_sound_manager->play(
      "wake_word_triggered.wav",
      volume);

  vipi_audio_manager.release(
      ViPiAudioManager::Owner::SYSTEM);
}

void ViPiAudio::setup() {
  load_settings_();

  // Apply fixed pins for known hardware profiles.
  // Custom profile keeps NVS pin assignments.
  apply_hardware_profile_();

  // Display is optional.
  // If disabled or physically absent, ViPi continues normally.
  if (display_type_ == 1) {
    if (!vipi_display_manager.begin(
            display_sda_,
            display_scl_,
            static_cast<uint8_t>(display_address_))) {
      ESP_LOGI(
          TAG,
          "OLED not present - display disabled for this boot");
    }
  } else if (display_type_ == 0) {
    ESP_LOGI(
        TAG,
        "Display disabled by configuration");
  } else {
    ESP_LOGI(
        TAG,
        "ST7789 configuration reserved for later");
  }

  ViPiLedConfig led_config;

  // Purple board + external 24 LED WS2812 ring.
  led_config.pin = led_pin_;
  led_config.count = led_count_;
  led_config.brightness =
      static_cast<float>(led_brightness_) / 100.0f;
  led_config.reverse = led_reverse_;
  led_config.offset = led_offset_;
  led_config.update_ms = 25;

  // WS2812 is fire-and-forget.
  // Co LED thi hien thi, khong gan LED thi ViPi van chay binh thuong.
  // Khong probe, khong cho ACK, khong retry phan cung.
  vipi_led_manager.begin(led_config);

  vipi_led_manager.set_state(
      ViPiLedState::BOOTING);

  // LED GPIO48 is owned by LedManager/WS2812.
  // Do not reset it as a normal GPIO after LedManager.begin().

  if (touch_enabled_) {
    vipi_touch_manager.begin(
        touch_vol_up_,
        touch_vol_down_,
        touch_action_);
  } else {
    ESP_LOGI(
        TAG,
        "Touch buttons disabled by configuration");
  }

  if (ptt_ >= 0) {
    gpio_reset_pin((gpio_num_t) ptt_);
    gpio_set_direction((gpio_num_t) ptt_, GPIO_MODE_INPUT);
    gpio_pullup_en((gpio_num_t) ptt_);
  }

  if (!init_i2s_() || !init_multinet_()) {
    mark_failed();
    return;
  }

  if (!vipi_audio_manager.begin(tx_chan)) {
    ESP_LOGE(TAG, "AudioManager init failed");
    mark_failed();
    return;
  }

  vipi_init_sound_manager();
  xTaskCreatePinnedToCore(audio_task_entry_, "vipi_audio", 12288, this, 5, nullptr, 1);
  xTaskCreatePinnedToCore(tcp_task_entry_, "vipi_tcp", 16384, this, 4, nullptr, 0);
  xTaskCreatePinnedToCore(music_task_entry_, "vipi_music", 16384, this, 4, (TaskHandle_t*)&music_task_handle_, 0);
}

void ViPiAudio::audio_task_entry_(void *p) { static_cast<ViPiAudio*>(p)->audio_task_(); vTaskDelete(nullptr); }
void ViPiAudio::tcp_task_entry_(void *p) { static_cast<ViPiAudio*>(p)->tcp_task_(); vTaskDelete(nullptr); }
void ViPiAudio::music_task_entry_(void *p) { static_cast<ViPiAudio*>(p)->music_task_(); vTaskDelete(nullptr); }


// ===== END vipi_lifecycle_impl.inc =====


// ===== BEGIN vipi_capture_impl.inc =====

void ViPiAudio::change_volume_(int delta) {
    int next = volume_ + delta;

    if (next < 1) {
        next = 1;
    }

    if (next > 20) {
        next = 20;
    }

    if (next == volume_) {
        return;
    }

    volume_ = next;

    save_settings_();

    vipi_led_manager.set_volume(
        static_cast<float>(volume_) / 20.0f);

    vipi_display_manager.show_volume(
        volume_,
        20);

    ESP_LOGI(
        TAG,
        "VOLUME: %d/20",
        volume_);
}

void ViPiAudio::action_button_() {
    if (playing) {
        ESP_LOGI(
            TAG,
            "ACTION: stop current playback");

        music_stop_ = true;
        return;
    }

    ESP_LOGI(
        TAG,
        "ACTION: start listening");

    vipi_led_manager.set_state(
        ViPiLedState::WAKE);

    vipi_display_manager.show_wake();

    vipi_play_wake_sound(volume_);

    vipi_led_manager.set_state(
        ViPiLedState::LISTENING);

    wake_stream = true;
    wake_until =
        millis() + wake_record_ms_;

    if (mn && mn_data) {
        mn->clean(mn_data);
    }
}

void ViPiAudio::handle_buttons_() {
    const auto event =
        vipi_touch_manager.update();

    switch (event) {
        case ViPiButtonEvent::VOL_UP:
            change_volume_(1);
            break;

        case ViPiButtonEvent::VOL_DOWN:
            change_volume_(-1);
            break;

        case ViPiButtonEvent::ACTION:
            action_button_();
            break;

        default:
            break;
    }
}

void ViPiAudio::audio_task_() {
  int chunk = mn->get_samp_chunksize(mn_data);
  int16_t *pcm = (int16_t*) heap_caps_malloc(chunk*sizeof(int16_t), MALLOC_CAP_8BIT);
  uint8_t *ulaw = (uint8_t*) heap_caps_malloc(chunk, MALLOC_CAP_8BIT);
  float dc = 0.0f;
  bool prev_send=false;

  sockaddr_in dst{};
  dst.sin_family=AF_INET; dst.sin_port=htons(3000);
  int udp_sock=socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);

  ESP_LOGI(TAG, "[vipi] Listening for '%s' chunk=%d", command_.c_str(), chunk);

  while (true) {

    vipi_led_manager.update();
    vipi_display_manager.update();
    handle_buttons_();

    size_t got=0;
    if (i2s_channel_read(rx_chan, pcm, chunk*sizeof(int16_t), &got, portMAX_DELAY) != ESP_OK || got != chunk*sizeof(int16_t)) continue;

    bool ptt = false;

    if (ptt_ >= 0) {
      ptt = gpio_get_level((gpio_num_t) ptt_) == 0;
    }
    if (wake_stream && (int32_t)(millis() - wake_until) >= 0) wake_stream=false;
    bool send_audio = !playing && (ptt || wake_stream);

    if (send_audio) {
      for (int i=0;i<chunk;i++) {
        dc += 0.001f * ((float)pcm[i]-dc);
        int32_t v=(int32_t)pcm[i]-(int32_t)dc;
        if (v > 32767) v = 32767;
        if (v < -32768) v = -32768;
        ulaw[i]=linear_to_ulaw((int16_t)v);
      }
      inet_pton(AF_INET, server_ip_.c_str(), &dst.sin_addr);
      sendto(udp_sock, ulaw, chunk, 0, (sockaddr*)&dst, sizeof(dst));
    } else if (!playing) {
      esp_mn_state_t state=mn->detect(mn_data, pcm);
      if (state==ESP_MN_STATE_DETECTED) {
        esp_mn_results_t *r=mn->get_results(mn_data);
        float prob=(r && r->num>0)?r->prob[0]:0.0f;
        ESP_LOGI(TAG, "====================================");
        ESP_LOGI(TAG, "VIPI WAKE DETECTED: %s prob=%.3f", command_.c_str(), prob);
        ESP_LOGI(TAG, "====================================");
        // Neu music dang chay, dung no truoc de SYSTEM lay I2S.
        // Buoc sau se nang cap thanh pause + resume dung vi tri.
        if (playing) {
          // Luu vi tri truoc khi nhuong speaker cho wake/TTS.
          if (media_resume_ctx.valid) {
            uint32_t elapsed =
                millis() - media_resume_ctx.started_ms;

            media_resume_ctx.position_ms += elapsed;
            media_resume_ctx.interrupted_by_assistant = true;

            media_resume_after_tts = true;

            ESP_LOGI(
                TAG,
                "MEDIA assistant pause at %lu ms",
                (unsigned long)
                    media_resume_ctx.position_ms);
          }

          music_stop_ = true;

          uint32_t wait_start = millis();

          while (playing &&
                 (millis() - wait_start) < 2000) {
            vTaskDelay(pdMS_TO_TICKS(10));
          }
        }

        vipi_led_manager.set_state(
            ViPiLedState::WAKE);

        vipi_display_manager.show_wake();

        vipi_play_wake_sound(volume_);

        vipi_led_manager.set_state(
            ViPiLedState::LISTENING);

        vipi_display_manager.show_listening();

        // Chi mo microphone sau khi am wake da ket thuc.
        wake_stream = true;
        wake_until = millis() + wake_record_ms_;
        set_led_(true);
        mn->clean(mn_data);
      } else if (state==ESP_MN_STATE_TIMEOUT) mn->clean(mn_data);
    }

    if (send_audio != prev_send) {
      ESP_LOGI(TAG, send_audio ? "Started recording -> UDP :3000" : "Stopped recording -> SEND");
      if (!send_audio) set_led_(false);
      prev_send=send_audio;
    }
  }
}

static bool recv_exact(int s, uint8_t *buf, size_t n, int timeout_ms) {
  size_t off=0;
  while (off<n) {
    fd_set rfds; FD_ZERO(&rfds); FD_SET(s,&rfds);
    timeval tv{timeout_ms/1000,(timeout_ms%1000)*1000};
    int r=select(s+1,&rfds,nullptr,nullptr,&tv);
    if (r<=0) return false;
    int k=recv(s,buf+off,n-off,0);
    if (k<=0) return false;
    off+=k;
  }
  return true;
}


// ===== END vipi_capture_impl.inc =====


// ===== BEGIN vipi_tts_impl.inc =====
void ViPiAudio::tcp_task_() {
  int err=0;
  OpusDecoder *dec=opus_decoder_create(16000,1,&err);
  if (!dec || err!=OPUS_OK) { ESP_LOGE(TAG,"Opus decoder failed %d",err); return; }

  int ls=socket(AF_INET,SOCK_STREAM,IPPROTO_IP);
  int yes=1; setsockopt(ls,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
  sockaddr_in a{}; a.sin_family=AF_INET; a.sin_port=htons(3001); a.sin_addr.s_addr=htonl(INADDR_ANY);
  if (bind(ls,(sockaddr*)&a,sizeof(a))<0 || listen(ls,2)<0) { ESP_LOGE(TAG,"TCP :3001 bind/listen failed"); return; }
  ESP_LOGI(TAG,"TCP speaker listening :3001");

  uint8_t pkt[4000]; int16_t pcm[320]; int32_t stereo[640];

  while (true) {
    int c=accept(ls,nullptr,nullptr);
    if (c<0) {
      continue;
    }

    // TTS co quyen doc quyen speaker trong ca utterance.
    // Media se khong duoc phep ghi I2S cung luc.
    if (!vipi_audio_manager.acquire(
            ViPiAudioManager::Owner::TTS,
            portMAX_DELAY)) {
      close(c);
      continue;
    }

    vipi_led_manager.set_state(
        ViPiLedState::SPEAKING);

    ESP_LOGI(TAG,"Server connected to speaker");
    uint8_t h[6];
    while (recv_exact(c,h,6,2000)) {
      if (h[0]!=0xAA) break;
      uint8_t volume=h[3] ? h[3] : (uint8_t) volume_;
      if (volume>20) volume=20;
      uint8_t compression=h[5];
      playing=true; wake_stream=false; set_led_(true);
      ESP_LOGI(TAG,"RX audio compression=%u volume=%u",compression,volume);

      if (compression==2) {
        while (true) {
          uint8_t lb[2]; if (!recv_exact(c,lb,2,2000)) break;
          uint16_t len=((uint16_t)lb[0]<<8)|lb[1];
          if (len==0) break;
          if (len>sizeof(pkt) || !recv_exact(c,pkt,len,2000)) break;
          int ns=opus_decode(dec,pkt,len,pcm,320,0);
          if (ns<0) { ESP_LOGW(TAG,"Opus decode %d",ns); continue; }
          for (int i=0;i<ns;i++) {
            int vol = static_cast<int>(volume);
            if (vol < 0) vol = 0;
            if (vol > 20) vol = 20;

            int32_t scaled =
                static_cast<int32_t>(
                    (static_cast<int64_t>(pcm[i]) * vol) / 20);

            int32_t v =
                static_cast<int32_t>(
                    static_cast<int64_t>(scaled) * 65536LL);
            if (v > 2147483647LL) v = 2147483647LL;
            if (v < -2147483648LL) v = -2147483648LL;
            stereo[i*2]=(int32_t)v; stereo[i*2+1]=(int32_t)v;
          }
          size_t wr=0; i2s_channel_write(tx_chan,stereo,ns*2*sizeof(int32_t),&wr,portMAX_DELAY);
        }
      } else {
        ESP_LOGW(TAG,"Only Opus compression=2 is enabled in this ViPi build");
      }
      int32_t zeros[640]={0}; size_t wr=0;
      i2s_channel_write(tx_chan,zeros,sizeof(zeros),&wr,portMAX_DELAY);
      playing=false; set_led_(false);
      ESP_LOGI(TAG,"Playback finished -> wake listening");
    }
    vipi_audio_manager.release(
        ViPiAudioManager::Owner::TTS);

    // TTS da ket thuc. Neu media chi bi assistant tam dung,
    // queue lai media sau khi TTS da release speaker.
    if (media_resume_after_tts &&
        media_resume_ctx.valid &&
        !media_user_stop) {

      media_resume_after_tts = false;

      std::string resume_url =
          media_resume_ctx.url;

      const char separator =
          resume_url.find('?') == std::string::npos
              ? '?'
              : '&';

      resume_url += separator;
      resume_url += "vipi_position_ms=";
      resume_url += std::to_string(
          media_resume_ctx.position_ms);

      music_url_ = resume_url;
      music_format_ = media_resume_ctx.format;

      media_resume_ctx.interrupted_by_assistant = false;

      music_pending_ = true;

      ESP_LOGI(
          TAG,
          "MEDIA resume queued at %lu ms",
          (unsigned long)
              media_resume_ctx.position_ms);
    }

    close(c);
  }
}



// ===== END vipi_tts_impl.inc =====


// ===== BEGIN vipi_settings_impl.inc =====
void ViPiAudio::apply_multinet_config_() {
  if (!mn || !mn_data) return;
  mn->set_det_threshold(mn_data, threshold_);
  esp_mn_commands_clear();
  esp_mn_commands_add(1, command_.c_str());
  esp_mn_commands_update();
  mn->clean(mn_data);
  ESP_LOGI(TAG, "Runtime config applied: wake='%s' threshold=%.2f server=%s",
           command_.c_str(), threshold_, server_ip_.c_str());
}

void ViPiAudio::load_settings_() {
  nvs_handle_t h;
  if (nvs_open("vipi", NVS_READONLY, &h) != ESP_OK) return;
  char buf[96]; size_t n;
  n=sizeof(buf); if (nvs_get_str(h,"server",buf,&n)==ESP_OK) server_ip_=buf;
  n=sizeof(buf); if (nvs_get_str(h,"wake",buf,&n)==ESP_OK) command_=buf;
  n=sizeof(buf); if (nvs_get_str(h,"room",buf,&n)==ESP_OK) room_=buf;
  int32_t i=0;

  if (nvs_get_i32(h,"hw_profile",&i)==ESP_OK)
    hardware_profile_=std::max(0,std::min(1,(int)i));
  if (nvs_get_i32(h,"threshold",&i)==ESP_OK) threshold_=std::max(0.05f,std::min(0.95f,i/100.0f));
  if (nvs_get_i32(h,"record_ms",&i)==ESP_OK) wake_record_ms_=std::max(1000,std::min(30000,(int)i));
  if (nvs_get_i32(h,"volume",&i)==ESP_OK)
    volume_=std::max(1,std::min(20,(int)i));

  // Audio GPIO for Custom profile.
  if (nvs_get_i32(h,"mic_ws",&i)==ESP_OK)
    mic_ws_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"mic_bclk",&i)==ESP_OK)
    mic_bclk_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"mic_din",&i)==ESP_OK)
    mic_din_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"spk_bclk",&i)==ESP_OK)
    spk_bclk_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"spk_ws",&i)==ESP_OK)
    spk_ws_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"spk_dout",&i)==ESP_OK)
    spk_dout_=std::max(0,std::min(48,(int)i));

  // LED configuration.
  if (nvs_get_i32(h,"led_pin",&i)==ESP_OK)
    led_pin_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"led_count",&i)==ESP_OK)
    led_count_=std::max(1,std::min(300,(int)i));

  if (nvs_get_i32(h,"led_bright",&i)==ESP_OK)
    led_brightness_=std::max(1,std::min(100,(int)i));

  if (nvs_get_i32(h,"led_reverse",&i)==ESP_OK)
    led_reverse_=(i != 0);

  if (nvs_get_i32(h,"led_offset",&i)==ESP_OK)
    led_offset_=std::max(0,std::min(299,(int)i));

  // Display configuration.
  if (nvs_get_i32(h,"disp_type",&i)==ESP_OK)
    display_type_=std::max(0,std::min(2,(int)i));

  if (nvs_get_i32(h,"disp_sda",&i)==ESP_OK)
    display_sda_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"disp_scl",&i)==ESP_OK)
    display_scl_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"disp_addr",&i)==ESP_OK)
    display_address_=std::max(0x08,std::min(0x77,(int)i));

  // Touch configuration.
  if (nvs_get_i32(h,"touch_en",&i)==ESP_OK)
    touch_enabled_=(i != 0);

  if (nvs_get_i32(h,"touch_up",&i)==ESP_OK)
    touch_vol_up_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"touch_down",&i)==ESP_OK)
    touch_vol_down_=std::max(0,std::min(48,(int)i));

  if (nvs_get_i32(h,"touch_action",&i)==ESP_OK)
    touch_action_=std::max(0,std::min(48,(int)i));

  nvs_close(h);
}

bool ViPiAudio::save_settings_() {
  nvs_handle_t h;
  if (nvs_open("vipi", NVS_READWRITE, &h) != ESP_OK) return false;
  esp_err_t e=ESP_OK;
  e=(esp_err_t)(e | nvs_set_str(h,"server",server_ip_.c_str()));
  e=(esp_err_t)(e | nvs_set_str(h,"wake",command_.c_str()));
  e=(esp_err_t)(e | nvs_set_str(h,"room",room_.c_str()));
  e=(esp_err_t)(e | nvs_set_i32(h,"threshold",(int32_t)(threshold_*100.0f+0.5f)));
  e=(esp_err_t)(e | nvs_set_i32(h,"record_ms",(int32_t)wake_record_ms_));
  e=(esp_err_t)(e | nvs_set_i32(h,"volume",volume_));
  e=(esp_err_t)(e | nvs_set_i32(h,"hw_profile",hardware_profile_));

  // Audio GPIO. Used by Custom profile.
  e=(esp_err_t)(e | nvs_set_i32(h,"mic_ws",mic_ws_));
  e=(esp_err_t)(e | nvs_set_i32(h,"mic_bclk",mic_bclk_));
  e=(esp_err_t)(e | nvs_set_i32(h,"mic_din",mic_din_));
  e=(esp_err_t)(e | nvs_set_i32(h,"spk_bclk",spk_bclk_));
  e=(esp_err_t)(e | nvs_set_i32(h,"spk_ws",spk_ws_));
  e=(esp_err_t)(e | nvs_set_i32(h,"spk_dout",spk_dout_));

  // LED.
  e=(esp_err_t)(e | nvs_set_i32(h,"led_pin",led_pin_));
  e=(esp_err_t)(e | nvs_set_i32(h,"led_count",led_count_));
  e=(esp_err_t)(e | nvs_set_i32(h,"led_bright",led_brightness_));
  e=(esp_err_t)(e | nvs_set_i32(h,"led_reverse",led_reverse_ ? 1 : 0));
  e=(esp_err_t)(e | nvs_set_i32(h,"led_offset",led_offset_));

  // Display.
  e=(esp_err_t)(e | nvs_set_i32(h,"disp_type",display_type_));
  e=(esp_err_t)(e | nvs_set_i32(h,"disp_sda",display_sda_));
  e=(esp_err_t)(e | nvs_set_i32(h,"disp_scl",display_scl_));
  e=(esp_err_t)(e | nvs_set_i32(h,"disp_addr",display_address_));

  // Touch.
  e=(esp_err_t)(e | nvs_set_i32(h,"touch_en",touch_enabled_ ? 1 : 0));
  e=(esp_err_t)(e | nvs_set_i32(h,"touch_up",touch_vol_up_));
  e=(esp_err_t)(e | nvs_set_i32(h,"touch_down",touch_vol_down_));
  e=(esp_err_t)(e | nvs_set_i32(h,"touch_action",touch_action_));

  if (e==ESP_OK)
    e=nvs_commit(h);
  nvs_close(h);
  return e==ESP_OK;
}


// ===== END vipi_settings_impl.inc =====


// ===== BEGIN vipi_web_impl.inc =====
std::string ViPiAudio::url_decode_(const std::string &s) {
  std::string o; o.reserve(s.size());
  for (size_t i=0;i<s.size();i++) {
    if (s[i]=='+') o.push_back(' ');
    else if (s[i]=='%' && i+2<s.size()) {
      unsigned int v=0; if (sscanf(s.substr(i+1,2).c_str(),"%02x",&v)==1) { o.push_back((char)v); i+=2; }
      else o.push_back(s[i]);
    } else o.push_back(s[i]);
  }
  return o;
}

std::string ViPiAudio::form_value_(const std::string &body, const char *key) {
  std::string k=std::string(key)+"="; size_t p=body.find(k);
  if (p==std::string::npos) return ""; p+=k.size(); size_t e=body.find('&',p);
  return url_decode_(body.substr(p,e==std::string::npos?std::string::npos:e-p));
}

static std::string html_escape(const std::string &s) {
  std::string o; for(char c:s){ if(c=='&')o+="&amp;"; else if(c=='<')o+="&lt;"; else if(c=='>')o+="&gt;"; else if(c=='\"')o+="&quot;"; else o+=c; } return o;
}

esp_err_t ViPiAudio::web_root_(httpd_req_t *req) {
  auto *self=(ViPiAudio*)req->user_ctx;
  std::string page=R"HTML(<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1"><meta charset=utf-8><title>ViPi V2</title><style>body{font-family:system-ui;margin:0;background:#f5f5f7;color:#222}.w{max-width:620px;margin:auto;padding:22px}.c{background:white;border-radius:18px;padding:20px;margin:14px 0;box-shadow:0 2px 12px #0001}h2{margin:0 0 12px;color:#6536a6}label{display:block;margin:12px 0 5px;font-weight:600}input,select{box-sizing:border-box;width:100%;padding:12px;border:1px solid #bbb;border-radius:10px;font-size:16px;background:white}.check{width:auto;margin-right:8px}button{padding:12px 16px;border:0;border-radius:10px;background:#6536a6;color:white;font-weight:700;font-size:15px;margin:8px 6px 0 0}.muted{color:#666;font-size:14px}.ok{color:#087f23}</style></head><body><div class=w><div class=c><h2>ViPi V2</h2><div class=ok>● Đang hoạt động</div><div class=muted>Wi‑Fi đã kết nối. Wi‑Fi được đổi qua ViPi-Setup khi mất kết nối.</div></div><form class=c method=post action=/save><label>Tên phòng / thiết bị</label><input name=room value=")HTML";
  page+=html_escape(self->room_); page+=R"HTML("><label>Máy chủ ViPi</label><input name=server value=")HTML";
  page+=html_escape(self->server_ip_); page+=R"HTML(" inputmode=decimal><button type=button onclick="findServer()">Tìm máy chủ</button><div id=find class=muted></div><label>Hotword MultiNet</label><input name=wake value=")HTML";
  page+=html_escape(self->command_); page+=R"HTML(" placeholder="V B"><label>Ngưỡng nhận (0.05–0.95)</label><input name=threshold type=number min=.05 max=.95 step=.01 value=")HTML";
  char b[32]; snprintf(b,sizeof(b),"%.2f",self->threshold_); page+=b; page+=R"HTML("><label>Thời gian nghe sau wake (ms)</label><input name=record_ms type=number min=1000 max=30000 step=500 value=")HTML";
  page+=std::to_string(self->wake_record_ms_); page+=R"HTML("><label>Âm lượng mặc định (1–20)</label><input name=volume type=number min=1 max=20 value=")HTML";
  page+=std::to_string(self->volume_);

  page+=R"HTML(">

  <h3>Phần cứng</h3>

  <label>Hardware Profile</label>
  <select name=hardware_profile>
    <option value=0)HTML";

  if (self->hardware_profile_ == 0)
    page+=" selected";

  page+=R"HTML(>Purple Board</option>
    <option value=1)HTML";

  if (self->hardware_profile_ == 1)
    page+=" selected";

  page+=R"HTML(>Custom</option>
  </select>

  <div class=muted>
    Purple Board dùng pin chuẩn cố định.
    Chọn Custom để thay đổi GPIO.
    Thay đổi phần cứng có hiệu lực sau reboot.
  </div>

  <h3>Audio GPIO</h3>

  <label>MIC WS</label>
  <input name=mic_ws type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->mic_ws_);

  page+=R"HTML(">

  <label>MIC BCLK</label>
  <input name=mic_bclk type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->mic_bclk_);

  page+=R"HTML(">

  <label>MIC DIN</label>
  <input name=mic_din type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->mic_din_);

  page+=R"HTML(">

  <label>Speaker BCLK</label>
  <input name=spk_bclk type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->spk_bclk_);

  page+=R"HTML(">

  <label>Speaker LRCK / WS</label>
  <input name=spk_ws type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->spk_ws_);

  page+=R"HTML(">

  <label>Speaker DOUT</label>
  <input name=spk_dout type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->spk_dout_);

  page+=R"HTML(">

  <h3>LED WS2812</h3>

  <label>GPIO LED</label>
  <input name=led_pin type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->led_pin_);

  page+=R"HTML(">

  <label>Số LED</label>
  <input name=led_count type=number min=1 max=300 value=")HTML";
  page+=std::to_string(self->led_count_);

  page+=R"HTML(">

  <label>Độ sáng LED (%)</label>
  <input name=led_bright type=number min=1 max=100 value=")HTML";
  page+=std::to_string(self->led_brightness_);

  page+=R"HTML(">

  <label>Đảo chiều LED</label>
  <select name=led_reverse>
    <option value=0)HTML";

  if (!self->led_reverse_)
    page+=" selected";

  page+=R"HTML(>Không</option>
    <option value=1)HTML";

  if (self->led_reverse_)
    page+=" selected";

  page+=R"HTML(>Có</option>
  </select>

  <label>LED Offset</label>
  <input name=led_offset type=number min=0 max=299 value=")HTML";
  page+=std::to_string(self->led_offset_);

  page+=R"HTML(">

  <h3>Màn hình</h3>

  <label>Loại màn hình</label>
  <select name=display_type>
    <option value=0)HTML";

  if (self->display_type_ == 0)
    page+=" selected";

  page+=R"HTML(>Không dùng</option>
    <option value=1)HTML";

  if (self->display_type_ == 1)
    page+=" selected";

  page+=R"HTML(>SSD1306 128x64 I2C</option>
    <option value=2)HTML";

  if (self->display_type_ == 2)
    page+=" selected";

  page+=R"HTML(>ST7789 SPI (sắp hỗ trợ)</option>
  </select>

  <label>OLED SDA</label>
  <input name=display_sda type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->display_sda_);

  page+=R"HTML(">

  <label>OLED SCL</label>
  <input name=display_scl type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->display_scl_);

  page+=R"HTML(">

  <label>OLED I2C Address</label>
  <input name=display_addr type=number min=8 max=119 value=")HTML";
  page+=std::to_string(self->display_address_);

  page+=R"HTML(">
  <div class=muted>60 = 0x3C, 61 = 0x3D</div>

  <h3>Nút điều khiển</h3>

  <label>Bật nút vật lý</label>
  <select name=touch_enabled>
    <option value=1)HTML";

  if (self->touch_enabled_)
    page+=" selected";

  page+=R"HTML(>Có</option>
    <option value=0)HTML";

  if (!self->touch_enabled_)
    page+=" selected";

  page+=R"HTML(>Không</option>
  </select>

  <label>GPIO Volume +</label>
  <input name=touch_up type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->touch_vol_up_);

  page+=R"HTML(">

  <label>GPIO Volume -</label>
  <input name=touch_down type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->touch_vol_down_);

  page+=R"HTML(">

  <label>GPIO Action</label>
  <input name=touch_action type=number min=0 max=48 value=")HTML";
  page+=std::to_string(self->touch_action_);

  page+=R"HTML(">

  <br>
  <button type=submit>Lưu cấu hình</button>
  </form><div class=c><b>Cập nhật firmware OTA</b><p class=muted>Chọn <b>VIPI_V2_CLIENT_APP.bin</b>. Không dùng FULL.bin ở đây.</p><input id=fw type=file accept=.bin><button type=button onclick="ota()">Cập nhật OTA</button><div id=ota class=muted></div></div><div class=c><b>Wi‑Fi lần đầu / đổi router</b><p class=muted>Nếu không kết nối được Wi‑Fi, ViPi phát điểm truy cập <b>ViPi-Setup</b>. Kết nối bằng mật khẩu <b>12345678</b>, sau đó mở <b>192.168.4.1</b> để nhập Wi‑Fi mới.</p></div></div><script>async function findServer(){let e=document.getElementById('find');e.textContent='Đang tìm...';try{let r=await fetch('/find');let j=await r.json();if(j.found){document.querySelector('[name=server]').value=j.ip;e.textContent='Đã tìm thấy '+j.ip}else e.textContent='Chưa thấy server. Có thể nhập IP thủ công.'}catch(x){e.textContent='Không tìm được.'}}async function ota(){let f=document.getElementById('fw').files[0],e=document.getElementById('ota');if(!f){e.textContent='Hãy chọn file APP.bin';return}if(!f.name.endsWith('.bin')){e.textContent='File không hợp lệ';return}if(!confirm('Cập nhật '+f.name+'? Không tắt nguồn trong khi cập nhật.'))return;e.textContent='Đang tải firmware...';try{let r=await fetch('/ota',{method:'POST',headers:{'Content-Type':'application/octet-stream','X-Filename':f.name},body:f});e.textContent=await r.text();if(r.ok)e.textContent+=' Thiết bị sẽ khởi động lại.'}catch(x){e.textContent='Mất kết nối khi reboot có thể là bình thường.'}}</script></body></html>)HTML";
  httpd_resp_set_type(req,"text/html; charset=utf-8");
  return httpd_resp_send(req,page.c_str(),page.size());
}



void ViPiAudio::apply_hardware_profile_() {
  if (hardware_profile_ != 0) {
    // Custom profile keeps values loaded from NVS/Web.
    return;
  }

  // ==========================================================
  // PURPLE BOARD
  // ==========================================================

  // Microphone I2S.
  mic_ws_ = 4;
  mic_bclk_ = 5;
  mic_din_ = 6;

  // Speaker I2S.
  spk_bclk_ = 15;
  spk_ws_ = 16;
  spk_dout_ = 7;

  // GPIO39 belongs to Volume-, therefore no PTT.
  ptt_ = -1;

  // External WS2812 ring.
  led_pin_ = 48;
  led_count_ = 24;
  led_brightness_ = 50;
  led_reverse_ = false;
  led_offset_ = 0;

  // SSD1306 128x64 I2C.
  display_type_ = 1;
  display_sda_ = 41;
  display_scl_ = 42;
  display_address_ = 0x3C;

  // Physical buttons.
  touch_enabled_ = true;
  touch_vol_up_ = 40;
  touch_vol_down_ = 39;
  touch_action_ = 0;
}

bool ViPiAudio::validate_gpio_config_(std::string &error) const {
  struct PinUse {
    int pin;
    const char *name;
    bool enabled;
  };

  const PinUse pins[] = {
      {mic_ws_, "MIC WS", true},
      {mic_bclk_, "MIC BCLK", true},
      {mic_din_, "MIC DIN", true},

      {spk_bclk_, "SPEAKER BCLK", true},
      {spk_ws_, "SPEAKER LRCK", true},
      {spk_dout_, "SPEAKER DOUT", true},

      // PTT is optional.
      {ptt_, "PTT", ptt_ >= 0},

      // LED WS2812 has no hardware detection.
      {led_pin_, "LED", led_pin_ >= 0},

      // SSD1306 uses SDA/SCL only when selected.
      {display_sda_, "DISPLAY SDA", display_type_ == 1},
      {display_scl_, "DISPLAY SCL", display_type_ == 1},

      // ST7789 pins will be added when its driver is implemented.

      {touch_vol_up_, "VOLUME +", touch_enabled_},
      {touch_vol_down_, "VOLUME -", touch_enabled_},
      {touch_action_, "ACTION", touch_enabled_},
  };

  constexpr size_t count =
      sizeof(pins) / sizeof(pins[0]);

  for (size_t i = 0; i < count; ++i) {
    if (!pins[i].enabled) {
      continue;
    }

    if (pins[i].pin < 0 || pins[i].pin > 48) {
      error =
          std::string(pins[i].name) +
          ": GPIO " +
          std::to_string(pins[i].pin) +
          " is outside allowed range 0..48";
      return false;
    }

    for (size_t j = i + 1; j < count; ++j) {
      if (!pins[j].enabled) {
        continue;
      }

      if (pins[i].pin == pins[j].pin) {
        error =
            "GPIO " +
            std::to_string(pins[i].pin) +
            " conflict: " +
            pins[i].name +
            " / " +
            pins[j].name;
        return false;
      }
    }
  }

  // OLED I2C address is relevant only for SSD1306.
  if (display_type_ == 1) {
    if (display_address_ < 0x08 ||
        display_address_ > 0x77) {
      error =
          "OLED I2C address must be between "
          "8 and 119";
      return false;
    }
  }

  // LED configuration sanity checks.
  if (led_count_ < 1 || led_count_ > 300) {
    error = "LED count must be between 1 and 300";
    return false;
  }

  if (led_brightness_ < 1 ||
      led_brightness_ > 100) {
    error =
        "LED brightness must be between 1 and 100";
    return false;
  }

  if (led_offset_ < 0 ||
      led_offset_ >= led_count_) {
    error =
        "LED offset must be smaller than LED count";
    return false;
  }

  error.clear();
  return true;
}

esp_err_t ViPiAudio::web_save_(httpd_req_t *req) {
  auto *self=(ViPiAudio*)req->user_ctx;
  std::string body; body.resize(req->content_len);
  int got=0; while(got<req->content_len){int r=httpd_req_recv(req,&body[got],req->content_len-got);if(r<=0)break;got+=r;} body.resize(got);
  std::string v;

  // Keep the current hardware configuration so an invalid
  // Web submission can be rejected without changing runtime state.
  const int old_hardware_profile =
      self->hardware_profile_;

  const int old_mic_ws = self->mic_ws_;
  const int old_mic_bclk = self->mic_bclk_;
  const int old_mic_din = self->mic_din_;

  const int old_spk_bclk = self->spk_bclk_;
  const int old_spk_ws = self->spk_ws_;
  const int old_spk_dout = self->spk_dout_;

  const int old_led_pin = self->led_pin_;
  const int old_led_count = self->led_count_;
  const int old_led_brightness = self->led_brightness_;
  const bool old_led_reverse = self->led_reverse_;
  const int old_led_offset = self->led_offset_;

  const int old_display_type = self->display_type_;
  const int old_display_sda = self->display_sda_;
  const int old_display_scl = self->display_scl_;
  const int old_display_address = self->display_address_;

  const bool old_touch_enabled = self->touch_enabled_;
  const int old_touch_vol_up = self->touch_vol_up_;
  const int old_touch_vol_down = self->touch_vol_down_;
  const int old_touch_action = self->touch_action_;
  v=form_value_(body,"server"); if(!v.empty()){ in_addr a{}; if(inet_pton(AF_INET,v.c_str(),&a)==1) self->server_ip_=v; }
  v=form_value_(body,"wake"); if(!v.empty() && v.size()<=63) self->command_=v;
  v=form_value_(body,"room"); if(!v.empty() && v.size()<=63) self->room_=v;
  v=form_value_(body,"threshold"); if(!v.empty()) self->threshold_=std::max(0.05f,std::min(0.95f,(float)atof(v.c_str())));
  v=form_value_(body,"record_ms"); if(!v.empty()) self->wake_record_ms_=std::max(1000,std::min(30000,atoi(v.c_str())));
  v=form_value_(body,"volume");
  if(!v.empty())
    self->volume_=std::max(1,std::min(20,atoi(v.c_str())));

  // Hardware profile.
  v=form_value_(body,"hardware_profile");
  if(!v.empty())
    self->hardware_profile_=
        std::max(0,std::min(1,atoi(v.c_str())));

  // Audio GPIO values are editable only in Custom profile.
  if (self->hardware_profile_ == 1) {
    v=form_value_(body,"mic_ws");
    if(!v.empty())
      self->mic_ws_=std::max(0,std::min(48,atoi(v.c_str())));

    v=form_value_(body,"mic_bclk");
    if(!v.empty())
      self->mic_bclk_=std::max(0,std::min(48,atoi(v.c_str())));

    v=form_value_(body,"mic_din");
    if(!v.empty())
      self->mic_din_=std::max(0,std::min(48,atoi(v.c_str())));

    v=form_value_(body,"spk_bclk");
    if(!v.empty())
      self->spk_bclk_=std::max(0,std::min(48,atoi(v.c_str())));

    v=form_value_(body,"spk_ws");
    if(!v.empty())
      self->spk_ws_=std::max(0,std::min(48,atoi(v.c_str())));

    v=form_value_(body,"spk_dout");
    if(!v.empty())
      self->spk_dout_=std::max(0,std::min(48,atoi(v.c_str())));
  } else {
    // Purple Board ignores submitted custom GPIO values.
    self->apply_hardware_profile_();
  }

  // LED.
  v=form_value_(body,"led_pin");
  if(!v.empty())
    self->led_pin_=std::max(0,std::min(48,atoi(v.c_str())));

  v=form_value_(body,"led_count");
  if(!v.empty())
    self->led_count_=std::max(1,std::min(300,atoi(v.c_str())));

  v=form_value_(body,"led_bright");
  if(!v.empty())
    self->led_brightness_=std::max(1,std::min(100,atoi(v.c_str())));

  v=form_value_(body,"led_reverse");
  if(!v.empty())
    self->led_reverse_=(atoi(v.c_str()) != 0);

  v=form_value_(body,"led_offset");
  if(!v.empty())
    self->led_offset_=std::max(0,std::min(299,atoi(v.c_str())));

  // Display.
  v=form_value_(body,"display_type");
  if(!v.empty())
    self->display_type_=std::max(0,std::min(2,atoi(v.c_str())));

  v=form_value_(body,"display_sda");
  if(!v.empty())
    self->display_sda_=std::max(0,std::min(48,atoi(v.c_str())));

  v=form_value_(body,"display_scl");
  if(!v.empty())
    self->display_scl_=std::max(0,std::min(48,atoi(v.c_str())));

  v=form_value_(body,"display_addr");
  if(!v.empty())
    self->display_address_=std::max(0x08,std::min(0x77,atoi(v.c_str())));

  // Touch.
  v=form_value_(body,"touch_enabled");
  if(!v.empty())
    self->touch_enabled_=(atoi(v.c_str()) != 0);

  v=form_value_(body,"touch_up");
  if(!v.empty())
    self->touch_vol_up_=std::max(0,std::min(48,atoi(v.c_str())));

  v=form_value_(body,"touch_down");
  if(!v.empty())
    self->touch_vol_down_=std::max(0,std::min(48,atoi(v.c_str())));

  v=form_value_(body,"touch_action");
  if(!v.empty())
    self->touch_action_=std::max(0,std::min(48,atoi(v.c_str())));

  // Known profiles always own their hardware pin mapping.
  // Ignore any manually submitted hardware fields.
  if (self->hardware_profile_ == 0) {
    self->apply_hardware_profile_();
  }

  std::string gpio_error;

  if (!self->validate_gpio_config_(gpio_error)) {
    // Restore previous hardware values.
    self->hardware_profile_ =
        old_hardware_profile;

    self->mic_ws_ = old_mic_ws;
    self->mic_bclk_ = old_mic_bclk;
    self->mic_din_ = old_mic_din;

    self->spk_bclk_ = old_spk_bclk;
    self->spk_ws_ = old_spk_ws;
    self->spk_dout_ = old_spk_dout;

    self->led_pin_ = old_led_pin;
    self->led_count_ = old_led_count;
    self->led_brightness_ = old_led_brightness;
    self->led_reverse_ = old_led_reverse;
    self->led_offset_ = old_led_offset;

    self->display_type_ = old_display_type;
    self->display_sda_ = old_display_sda;
    self->display_scl_ = old_display_scl;
    self->display_address_ = old_display_address;

    self->touch_enabled_ = old_touch_enabled;
    self->touch_vol_up_ = old_touch_vol_up;
    self->touch_vol_down_ = old_touch_vol_down;
    self->touch_action_ = old_touch_action;

    ESP_LOGW(
        TAG,
        "GPIO config rejected: %s",
        gpio_error.c_str());

    httpd_resp_set_status(
        req,
        "400 Bad Request");

    httpd_resp_set_type(
        req,
        "text/plain; charset=utf-8");

    std::string response =
        "Khong luu cau hinh. " +
        gpio_error;

    return httpd_resp_send(
        req,
        response.c_str(),
        response.size());
  }

  bool ok=self->save_settings_();

  // Wake/server changes can apply immediately.
  // Hardware pin changes take effect after reboot.
  self->config_dirty_=true;
  httpd_resp_set_status(req, ok?"303 See Other":"500 Internal Server Error");
  if(ok) httpd_resp_set_hdr(req,"Location","/");
  return httpd_resp_sendstr(req,ok?"Saved":"NVS save failed");
}

bool ViPiAudio::auto_find_server_() {
  int s=socket(AF_INET,SOCK_DGRAM,IPPROTO_IP); if(s<0)return false;
  int yes=1; setsockopt(s,SOL_SOCKET,SO_BROADCAST,&yes,sizeof(yes));
  timeval tv{0,700000}; setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
  sockaddr_in d{}; d.sin_family=AF_INET; d.sin_port=htons(12346); d.sin_addr.s_addr=htonl(INADDR_BROADCAST);
  std::string msg="VIPI_FIND_SERVER "+room_; sendto(s,msg.data(),msg.size(),0,(sockaddr*)&d,sizeof(d));
  char buf[128]={0}; sockaddr_in from{}; socklen_t fl=sizeof(from);
  int n=recvfrom(s,buf,sizeof(buf)-1,0,(sockaddr*)&from,&fl); close(s);
  if(n<=0)return false; buf[n]=0;
  if(strncmp(buf,"VIPI_SERVER",11)!=0)return false;
  char ip[INET_ADDRSTRLEN]; inet_ntop(AF_INET,&from.sin_addr,ip,sizeof(ip)); server_ip_=ip; save_settings_(); discovery_sent_=false;
  ESP_LOGI(TAG,"Auto-discovered server: %s",server_ip_.c_str()); return true;
}

esp_err_t ViPiAudio::web_find_(httpd_req_t *req) {
  auto *self=(ViPiAudio*)req->user_ctx; bool ok=self->auto_find_server_();
  std::string j=std::string("{\"found\":")+(ok?"true":"false")+",\"ip\":\""+self->server_ip_+"\"}";
  httpd_resp_set_type(req,"application/json"); return httpd_resp_send(req,j.c_str(),j.size());
}

esp_err_t ViPiAudio::web_status_(httpd_req_t *req) {
  auto *self=(ViPiAudio*)req->user_ctx;
  std::string j="{\"server\":\""+self->server_ip_+"\",\"wake\":\""+self->command_+"\",\"room\":\""+self->room_+"\"}";
  httpd_resp_set_type(req,"application/json"); return httpd_resp_send(req,j.c_str(),j.size());
}

void ViPiAudio::start_web_() {
  httpd_config_t cfg=HTTPD_DEFAULT_CONFIG(); cfg.server_port=80; cfg.max_uri_handlers=10;
  if(httpd_start(&web_server_,&cfg)!=ESP_OK){ ESP_LOGW(TAG,"Web setup failed to start"); return; }
  httpd_uri_t root{}; root.uri="/"; root.method=HTTP_GET; root.handler=web_root_; root.user_ctx=this; httpd_register_uri_handler(web_server_,&root);
  httpd_uri_t save{}; save.uri="/save"; save.method=HTTP_POST; save.handler=web_save_; save.user_ctx=this; httpd_register_uri_handler(web_server_,&save);
  httpd_uri_t find{}; find.uri="/find"; find.method=HTTP_GET; find.handler=web_find_; find.user_ctx=this; httpd_register_uri_handler(web_server_,&find);
  httpd_uri_t stat{}; stat.uri="/status"; stat.method=HTTP_GET; stat.handler=web_status_; stat.user_ctx=this; httpd_register_uri_handler(web_server_,&stat);
  httpd_uri_t mp{}; mp.uri="/music/play"; mp.method=HTTP_POST; mp.handler=web_music_play_; mp.user_ctx=this; httpd_register_uri_handler(web_server_,&mp);
  httpd_uri_t ms{}; ms.uri="/music/stop"; ms.method=HTTP_POST; ms.handler=web_music_stop_; ms.user_ctx=this; httpd_register_uri_handler(web_server_,&ms);
  httpd_uri_t ota{}; ota.uri="/ota"; ota.method=HTTP_POST; ota.handler=web_ota_; ota.user_ctx=this; httpd_register_uri_handler(web_server_,&ota);
  web_started_=true; ESP_LOGI(TAG,"V2 Web Setup + OTA: http://<device-ip>/");
}


esp_err_t ViPiAudio::web_ota_(httpd_req_t *req) {

  vipi_led_manager.set_state(
      ViPiLedState::OTA);

  vipi_display_manager.show_ota();

  // Raw application binary upload. The browser sends the selected APP.bin
  // directly as application/octet-stream, so no multipart parser is needed.
  if (req->content_len <= 0 || req->content_len > 0x380000) {
    httpd_resp_set_status(req, "400 Bad Request");
    return httpd_resp_sendstr(req, "Firmware size invalid");
  }
  const esp_partition_t *part = esp_ota_get_next_update_partition(nullptr);
  if (!part) {
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "No OTA partition");
  }
  ESP_LOGI(TAG, "OTA begin: %d bytes -> %s", (int)req->content_len, part->label);
  esp_ota_handle_t handle = 0;
  esp_err_t err = esp_ota_begin(part, req->content_len, &handle);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(err));
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "OTA begin failed");
  }
  uint8_t *buf = (uint8_t*)heap_caps_malloc(4096, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
  if (!buf) {
    esp_ota_abort(handle);
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "OTA buffer alloc failed");
  }
  int remain = req->content_len;
  while (remain > 0) {
    int want = remain > 4096 ? 4096 : remain;
    int n = httpd_req_recv(req, (char*)buf, want);
    if (n == HTTPD_SOCK_ERR_TIMEOUT) continue;
    if (n <= 0) { err = ESP_FAIL; break; }
    err = esp_ota_write(handle, buf, n);
    if (err != ESP_OK) break;
    remain -= n;
  }
  heap_caps_free(buf);
  if (err == ESP_OK) err = esp_ota_end(handle); else esp_ota_abort(handle);
  if (err == ESP_OK) err = esp_ota_set_boot_partition(part);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(err));
    httpd_resp_set_status(req, "500 Internal Server Error");
    return httpd_resp_sendstr(req, "OTA failed");
  }
  httpd_resp_sendstr(req, "OTA OK.");
  ESP_LOGI(TAG, "OTA complete, rebooting...");
  vTaskDelay(pdMS_TO_TICKS(800));
  esp_restart();
  return ESP_OK;
}



// ===== END vipi_web_impl.inc =====


// ===== BEGIN vipi_media_impl.inc =====
void ViPiAudio::request_music_(const std::string &url, const std::string &format) {
  media_user_stop = true;
  clear_media_resume_context();
  media_user_stop = false;
  music_stop_ = true;
  vTaskDelay(pdMS_TO_TICKS(30));
  music_url_ = url;
  music_format_ = format.empty() ? "mp3" : format;
  std::transform(music_format_.begin(), music_format_.end(), music_format_.begin(), ::tolower);
  music_stop_ = false;
  music_pending_ = true;
  ESP_LOGI(TAG, "MUSIC queued format=%s url=%.100s", music_format_.c_str(), url.c_str());
}

void ViPiAudio::stop_music_() {
  media_user_stop = true;
  clear_media_resume_context();
  music_stop_ = true;
  music_pending_ = false;
}

esp_err_t ViPiAudio::web_music_play_(httpd_req_t *req) {
  auto *self=(ViPiAudio*)req->user_ctx;
  std::string body; body.resize(req->content_len);
  int got=0; while(got<req->content_len){int r=httpd_req_recv(req,&body[got],req->content_len-got);if(r<=0)break;got+=r;} body.resize(got);
  std::string url=form_value_(body,"url");
  std::string fmt=form_value_(body,"format");
  if(url.empty()) { httpd_resp_set_status(req,"400 Bad Request"); return httpd_resp_sendstr(req,"missing url"); }
  self->request_music_(url, fmt);
  httpd_resp_set_type(req,"application/json");
  return httpd_resp_sendstr(req,"{\"ok\":true}");
}

esp_err_t ViPiAudio::web_music_stop_(httpd_req_t *req) {
  media_user_stop = true;
  clear_media_resume_context();
  auto *self=(ViPiAudio*)req->user_ctx; self->stop_music_();
  httpd_resp_set_type(req,"application/json");
  return httpd_resp_sendstr(req,"{\"ok\":true}");
}

static esp_audio_simple_dec_type_t music_type_from_format(const std::string &fmt) {
  if (fmt == "m4a" || fmt == "mp4") return ESP_AUDIO_SIMPLE_DEC_TYPE_M4A;
  if (fmt == "aac") return ESP_AUDIO_SIMPLE_DEC_TYPE_AAC;
  return ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
}



static void vipi_heap_failed_alloc_cb(size_t size, uint32_t caps, const char *func) {
  ESP_LOGE("vipi_heap",
           "ALLOC FAILED size=%u caps=0x%08x func=%s internal_free=%u largest_internal=%u psram_free=%u largest_psram=%u",
           (unsigned) size,
           (unsigned) caps,
           func ? func : "?",
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
           (unsigned) heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
}

void ViPiAudio::music_task_() {
  static bool heap_cb_registered = false;
  if (!heap_cb_registered) {
    heap_caps_register_failed_alloc_callback(vipi_heap_failed_alloc_cb);
    heap_cb_registered = true;
  }

  // Espressif simple decoder accepts arbitrary network chunks and parses
  // MP3/AAC/M4A frames/containers internally.
  ViPiDiagnostics::module("CODEC", "register decoders START");
  ViPiDiagnostics::memory("CODEC-REGISTER-BEFORE");

  ESP_LOGI("VIPI.CODEC", "NORMAL DEC REGISTER START");
  ViPiDiagnostics::memory("NORMAL-DEC-BEFORE");

  esp_audio_err_t normal_ret = esp_audio_dec_register_default();

  ESP_LOGI("VIPI.CODEC", "NORMAL DEC REGISTER DONE ret=%d", (int) normal_ret);
  ViPiDiagnostics::memory("NORMAL-DEC-AFTER");

  ESP_LOGI("VIPI.CODEC", "SIMPLE DEC REGISTER START");
  ViPiDiagnostics::memory("SIMPLE-DEC-BEFORE");

  esp_audio_err_t simple_ret = esp_audio_simple_dec_register_default();

  ESP_LOGI("VIPI.CODEC", "SIMPLE DEC REGISTER DONE ret=%d", (int) simple_ret);
  ViPiDiagnostics::memory("SIMPLE-DEC-AFTER");

  ViPiDiagnostics::module("CODEC", "register decoders DONE");
  ViPiDiagnostics::memory("CODEC-REGISTER-AFTER");

  const int IN_SIZE=4096;
  int out_cap=16384;
  uint8_t *in=(uint8_t*)heap_caps_malloc(IN_SIZE,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  uint8_t *out=(uint8_t*)heap_caps_malloc(out_cap,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  int32_t *stereo=(int32_t*)heap_caps_malloc(32768,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!in||!out||!stereo){
    ViPiDiagnostics::error("MEDIA", "codec buffers allocation FAILED");
    ESP_LOGE(TAG,"MUSIC codec buffers alloc failed");
    return;
  }

  ViPiDiagnostics::module("MEDIA", "codec buffers allocated");
  ViPiDiagnostics::memory("MEDIA-BUFFERS-READY");

  while(true){
    if(!music_pending_.exchange(false)){ vTaskDelay(pdMS_TO_TICKS(50)); continue; }

    std::string url=music_url_;
    std::string fmt=music_format_;
    auto dtype=music_type_from_format(fmt);
    music_stop_=false;
    playing=true;

    vipi_led_manager.set_state(
        ViPiLedState::MUSIC);

    wake_stream=false;
    set_led_(true);

    media_user_stop = false;

    media_resume_ctx.url = url;
    media_resume_ctx.format = fmt;
    media_resume_ctx.started_ms = millis();

    if (!media_resume_ctx.interrupted_by_assistant) {
      media_resume_ctx.position_ms = 0;
    }

    media_resume_ctx.valid = true;
    ESP_LOGI(TAG,"MUSIC DIRECT start format=%s %.100s",fmt.c_str(),url.c_str());
    ViPiDiagnostics::module("MEDIA", "PLAY command received");
    ViPiDiagnostics::memory("MEDIA-PLAY");

    esp_http_client_config_t hcfg={};
    hcfg.url=url.c_str();
    hcfg.timeout_ms=12000;
    hcfg.buffer_size=1024;
    hcfg.buffer_size_tx=2048;  // V2: YouTube uses a short LAN proxy URL
    hcfg.keep_alive_enable=true;
    // HTTPS streams from YouTube/Google require a verification method.
    // Use ESP-IDF's built-in Mozilla CA certificate bundle instead of
    // disabling TLS verification. This keeps direct M4A/AAC streaming secure.
    if(url.rfind("https://", 0) == 0) {
      hcfg.crt_bundle_attach = esp_crt_bundle_attach;
    }
    ViPiDiagnostics::module("HTTP", "OPEN START");
    ViPiDiagnostics::memory("HTTP-BEFORE");

    esp_http_client_handle_t h=esp_http_client_init(&hcfg);
    if(!h || esp_http_client_open(h,0)!=ESP_OK){
      ViPiDiagnostics::error("HTTP", "OPEN FAILED");
      ESP_LOGE(TAG,"MUSIC HTTP open failed"); if(h)esp_http_client_cleanup(h);
      playing=false; set_led_(false); continue;
    }
    int64_t content_len = esp_http_client_fetch_headers(h);
    int http_status = esp_http_client_get_status_code(h);

    ESP_LOGI(
        "VIPI.HTTP",
        "OPEN OK status=%d content_length=%lld",
        http_status,
        (long long)content_len
    );
    ViPiDiagnostics::memory("HTTP-OPEN-OK");

    // Zeroed codec-specific config is valid for normal AAC-LC/M4A playback.
    // The default registry provides both frame decoder and M4A parser.
    union { esp_m4a_dec_cfg_t m4a; esp_aac_dec_cfg_t aac; } codec_cfg{};
    esp_audio_simple_dec_cfg_t dcfg={};
    dcfg.dec_type=dtype; dcfg.dec_cfg=&codec_cfg; dcfg.use_frame_dec=false;
    if(dtype==ESP_AUDIO_SIMPLE_DEC_TYPE_M4A){ codec_cfg.m4a.aac_plus_enable=true; dcfg.cfg_size=sizeof(codec_cfg.m4a); }
    else if(dtype==ESP_AUDIO_SIMPLE_DEC_TYPE_AAC){ codec_cfg.aac.aac_plus_enable=true; dcfg.cfg_size=sizeof(codec_cfg.aac); }
    ESP_LOGI(TAG,
      "MUSIC HEAP before open: internal=%u largest_internal=%u psram=%u largest_psram=%u",
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
      (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
      (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if(dtype==ESP_AUDIO_SIMPLE_DEC_TYPE_M4A) {
      esp_m4a_enable_parse_log(true);  // VIPI M4A DIAG
    }

    esp_audio_simple_dec_handle_t dec=nullptr;

    ESP_LOGI(
        "VIPI.CODEC",
        "OPEN START format=%s type=%d cfg_size=%u",
        fmt.c_str(),
        (int)dtype,
        (unsigned)dcfg.cfg_size
    );
    ViPiDiagnostics::memory("DECODER-BEFORE-OPEN");

    esp_audio_err_t derr=esp_audio_simple_dec_open(&dcfg,&dec);

    ESP_LOGI(
        "VIPI.CODEC",
        "OPEN RESULT ret=%d handle=%p",
        (int)derr,
        dec
    );
    ViPiDiagnostics::memory("DECODER-AFTER-OPEN");
    if(derr!=ESP_AUDIO_ERR_OK || !dec){
      ViPiDiagnostics::error("CODEC", "decoder OPEN FAILED");
      ESP_LOGE(TAG,"MUSIC decoder open failed format=%s ret=%d",fmt.c_str(),(int)derr);
      esp_http_client_close(h); esp_http_client_cleanup(h); playing=false; set_led_(false); continue;
    }

    // ===== VIPI FIXED I2S 16KHZ =====
    // TTS va I2S loa deu chay 16kHz.
    // Nhac 44.1kHz se duoc resample xuong 16kHz.
    // Tuyet doi khong doi clock I2S trong luc phat.
    int current_hz=0;
    int current_ch=0;
    bool failed=false;

    // Linear resampler state: input -> fixed 16kHz output.
    double rs_pos=0.0;
    int16_t rs_prev=0;
    bool rs_have_prev=false;

    auto resample_to_16k = [&](const int16_t *src, int src_n, int src_hz,
                               int16_t *dst, int dst_cap) -> int {
      if(src_n<=0) return 0;

      if(src_hz==16000) {
        int n=std::min(src_n,dst_cap);
        memcpy(dst,src,n*sizeof(int16_t));
        return n;
      }

      if(src_hz<=0) return 0;

      const double step=(double)src_hz/16000.0;
      int out_n=0;

      // Keep one previous input sample so frame boundaries remain continuous.
      while(out_n<dst_cap) {
        int i=(int)rs_pos;

        if(i>=src_n) break;

        int i2=i+1;
        int16_t a=(i==0 && rs_have_prev)?rs_prev:src[i];
        int16_t b;

        if(i2<src_n) {
          b=src[i2];
        } else {
          b=src[i];
        }

        double frac=rs_pos-(double)i;
        double v=(double)a + ((double)b-(double)a)*frac;

        if(v>32767.0) v=32767.0;
        if(v<-32768.0) v=-32768.0;

        dst[out_n++]=(int16_t)v;
        rs_pos += step;
      }

      // Convert position to next-frame-relative coordinates.
      int consumed=(int)rs_pos;
      rs_pos -= consumed;

      if(src_n>0) {
        rs_prev=src[src_n-1];
        rs_have_prev=true;
      }

      return out_n;
    };
    bool first_input_logged = false;
    bool first_pcm_logged = false;

    while(!music_stop_){
      int n=esp_http_client_read(h,(char*)in,IN_SIZE);

      if(n > 0 && !first_input_logged){
        first_input_logged=true;
        ESP_LOGI("VIPI.HTTP", "FIRST DATA bytes=%d", n);
        ViPiDiagnostics::memory("MEDIA-FIRST-DATA");
      }

      if(n<0){
        ViPiDiagnostics::error("HTTP", "READ FAILED");
        failed=true;
        break;
      }
      if(n==0){
        if(!esp_http_client_is_complete_data_received(h)){ vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        break;
      }
      esp_audio_simple_dec_raw_t raw={}; raw.buffer=in; raw.len=n; raw.eos=false;
      while(raw.len>0 && !music_stop_){
        esp_audio_simple_dec_out_t frame={}; frame.buffer=out; frame.len=out_cap;
        static bool first_process_log=true;
        if(first_process_log){
          ESP_LOGI(TAG,
            "MUSIC HEAP before process: internal=%u largest_internal=%u psram=%u largest_psram=%u",
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT),
            (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
          first_process_log=false;
        }
        derr=esp_audio_simple_dec_process(dec,&raw,&frame);

        if(derr==ESP_AUDIO_ERR_BUFF_NOT_ENOUGH){
          int need=frame.needed_size;
          if(need<=out_cap || need>131072){
            ESP_LOGE(TAG,"MUSIC bad output need=%d",need);
            ViPiDiagnostics::error("CODEC", "invalid output buffer requirement");
            failed=true;
            break;
          }
          uint8_t *b=(uint8_t*)heap_caps_realloc(out,need,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
          if(!b){ ESP_LOGE(TAG,"MUSIC output realloc failed %d",need); failed=true; break; }
          out=b; out_cap=need; continue;
        }
        if(derr!=ESP_AUDIO_ERR_OK){
          ESP_LOGE(TAG,"MUSIC decode failed ret=%d",(int)derr);
          ViPiDiagnostics::error("CODEC", "DECODE FAILED");
          failed=true;
          break;
        }

        if(frame.decoded_size>0){
          if(!first_pcm_logged){
            first_pcm_logged=true;
            ESP_LOGI(
                "VIPI.MEDIA",
                "FIRST PCM decoded_size=%d",
                (int)frame.decoded_size
            );
            ViPiDiagnostics::memory("FIRST-PCM");
          }
          esp_audio_simple_dec_info_t info={}; esp_audio_simple_dec_get_info(dec,&info);
          if(info.bits_per_sample!=16 || info.sample_rate==0 || info.channel==0){
            ESP_LOGE(TAG,"MUSIC unsupported PCM %uHz %ubit ch=%u",(unsigned)info.sample_rate,info.bits_per_sample,info.channel); failed=true; break;
          }
          current_hz=(int)info.sample_rate;
          current_ch=(int)info.channel;

          int in_samples=frame.decoded_size/(2*info.channel);
          if(in_samples>4096) in_samples=4096;

          const int16_t *pcm=(const int16_t*)frame.buffer;

          // Convert source to mono first.
          static int16_t mono_buf[4096];
          for(int i=0;i<in_samples;i++){
            int32_t mono=(info.channel>=2)
              ? ((int32_t)pcm[i*info.channel]+(int32_t)pcm[i*info.channel+1])/2
              : pcm[i];

            if(mono>32767) mono=32767;
            if(mono<-32768) mono=-32768;
            mono_buf[i]=(int16_t)mono;
          }

          // I2S is permanently 16kHz.
          static int16_t out16[4096];
          int samples=resample_to_16k(
              mono_buf,
              in_samples,
              current_hz,
              out16,
              4096);

          int vol=std::max(0,std::min(20,volume_));

          for(int i=0;i<samples;i++){
            int32_t scaled=(int32_t)(((int64_t)out16[i]*vol)/20);
            int32_t v=scaled << 16;
            stereo[i*2]=v;
            stereo[i*2+1]=v;
          }

          size_t wr=0;
          size_t want=samples*2*sizeof(int32_t);
          esp_err_t iw=i2s_channel_write(tx_chan,stereo,want,&wr,portMAX_DELAY);
          if(iw!=ESP_OK || wr!=want){
            ESP_LOGW(TAG,
              "MUSIC I2S WRITE iw=%d wr=%u want=%u samples=%d hz=%u",
              (int)iw,(unsigned)wr,(unsigned)want,samples,
              (unsigned)info.sample_rate);
          }
        }
        if(raw.consumed<=0){ break; }
        raw.len-=raw.consumed; raw.buffer+=raw.consumed;
      }
      if(failed) break;
    }
    esp_audio_simple_dec_close(dec);
    esp_http_client_close(h); esp_http_client_cleanup(h);
    // Drain a short silent tail. I2S clock stays at 16kHz.
    int32_t z[640]={0};
    size_t wr=0;
    i2s_channel_write(tx_chan,z,sizeof(z),&wr,portMAX_DELAY);

    playing=false;
    set_led_(false);
    music_stop_=false;
    ESP_LOGI(TAG,"MUSIC DIRECT end format=%s%s",fmt.c_str(),failed?" ERROR":"");
  }
}


// ===== END vipi_media_impl.inc =====


// ===== BEGIN vipi_web_stop_impl.inc =====
void ViPiAudio::stop_web_() { if(web_server_){httpd_stop(web_server_);web_server_=nullptr;} web_started_=false; }


// ===== END vipi_web_stop_impl.inc =====


void ViPiAudio::dump_config() {
  ESP_LOGCONFIG(TAG,"ViPi VP Audio:");
  ESP_LOGCONFIG(TAG,"  Server: %s",server_ip_.c_str());
  ESP_LOGCONFIG(TAG,"  Room: %s / Web Setup enabled",room_.c_str());
  ESP_LOGCONFIG(TAG,"  Wake: '%s' threshold %.2f",command_.c_str(),threshold_);
  ESP_LOGCONFIG(TAG,"  UDP mic :3000 / TCP speaker :3001");
  ESP_LOGCONFIG(TAG,"  MIC WS/BCLK/DIN %d/%d/%d",mic_ws_,mic_bclk_,mic_din_);
  ESP_LOGCONFIG(TAG,"  SPK BCLK/WS/DOUT %d/%d/%d",spk_bclk_,spk_ws_,spk_dout_);
  ESP_LOGCONFIG(TAG,"  PTT GPIO%d / LED GPIO%d",ptt_,led_pin_);
}
}  // namespace vipi_audio
}  // namespace esphome
