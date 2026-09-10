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
        wake_stream=true; wake_until=millis()+wake_record_ms_;
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

