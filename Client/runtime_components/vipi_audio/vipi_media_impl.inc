void ViPiAudio::request_music_(const std::string &url, const std::string &format) {
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
  auto *self=(ViPiAudio*)req->user_ctx; self->stop_music_();
  httpd_resp_set_type(req,"application/json");
  return httpd_resp_sendstr(req,"{\"ok\":true}");
}

static esp_audio_simple_dec_type_t music_type_from_format(const std::string &fmt) {
  if (fmt == "m4a" || fmt == "mp4") return ESP_AUDIO_SIMPLE_DEC_TYPE_M4A;
  if (fmt == "aac") return ESP_AUDIO_SIMPLE_DEC_TYPE_AAC;
  return ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
}

void ViPiAudio::music_task_() {
  // Espressif simple decoder accepts arbitrary network chunks and parses
  // MP3/AAC/M4A frames/containers internally.
  esp_audio_dec_register_default();
  esp_audio_simple_dec_register_default();

  const int IN_SIZE=4096;
  int out_cap=16384;
  uint8_t *in=(uint8_t*)heap_caps_malloc(IN_SIZE,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  uint8_t *out=(uint8_t*)heap_caps_malloc(out_cap,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  int32_t *stereo=(int32_t*)heap_caps_malloc(32768,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!in||!out||!stereo){ ESP_LOGE(TAG,"MUSIC codec buffers alloc failed"); return; }

  while(true){
    if(!music_pending_.exchange(false)){ vTaskDelay(pdMS_TO_TICKS(50)); continue; }
    std::string url=music_url_;
    std::string fmt=music_format_;
    auto dtype=music_type_from_format(fmt);
    music_stop_=false; playing=true; wake_stream=false; set_led_(true);
    ESP_LOGI(TAG,"MUSIC DIRECT start format=%s %.100s",fmt.c_str(),url.c_str());

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
    esp_http_client_handle_t h=esp_http_client_init(&hcfg);
    if(!h || esp_http_client_open(h,0)!=ESP_OK){
      ESP_LOGE(TAG,"MUSIC HTTP open failed"); if(h)esp_http_client_cleanup(h);
      playing=false; set_led_(false); continue;
    }
    esp_http_client_fetch_headers(h);

    // Zeroed codec-specific config is valid for normal AAC-LC/M4A playback.
    // The default registry provides both frame decoder and M4A parser.
    union { esp_m4a_dec_cfg_t m4a; esp_aac_dec_cfg_t aac; } codec_cfg{};
    esp_audio_simple_dec_cfg_t dcfg={};
    dcfg.dec_type=dtype; dcfg.dec_cfg=&codec_cfg; dcfg.use_frame_dec=false;
    if(dtype==ESP_AUDIO_SIMPLE_DEC_TYPE_M4A){ codec_cfg.m4a.aac_plus_enable=true; dcfg.cfg_size=sizeof(codec_cfg.m4a); }
    else if(dtype==ESP_AUDIO_SIMPLE_DEC_TYPE_AAC){ codec_cfg.aac.aac_plus_enable=true; dcfg.cfg_size=sizeof(codec_cfg.aac); }
    esp_audio_simple_dec_handle_t dec=nullptr;
    esp_audio_err_t derr=esp_audio_simple_dec_open(&dcfg,&dec);
    if(derr!=ESP_AUDIO_ERR_OK || !dec){
      ESP_LOGE(TAG,"MUSIC decoder open failed format=%s ret=%d",fmt.c_str(),(int)derr);
      esp_http_client_close(h); esp_http_client_cleanup(h); playing=false; set_led_(false); continue;
    }

    int current_hz=0; int current_ch=0; bool failed=false;
    while(!music_stop_){
      int n=esp_http_client_read(h,(char*)in,IN_SIZE);
      if(n<0){ failed=true; break; }
      if(n==0){
        if(!esp_http_client_is_complete_data_received(h)){ vTaskDelay(pdMS_TO_TICKS(10)); continue; }
        break;
      }
      esp_audio_simple_dec_raw_t raw={}; raw.buffer=in; raw.len=n; raw.eos=false;
      while(raw.len>0 && !music_stop_){
        esp_audio_simple_dec_out_t frame={}; frame.buffer=out; frame.len=out_cap;
        derr=esp_audio_simple_dec_process(dec,&raw,&frame);
        if(derr==ESP_AUDIO_ERR_BUFF_NOT_ENOUGH){
          int need=frame.needed_size;
          if(need<=out_cap || need>131072){ ESP_LOGE(TAG,"MUSIC bad output need=%d",need); failed=true; break; }
          uint8_t *b=(uint8_t*)heap_caps_realloc(out,need,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
          if(!b){ ESP_LOGE(TAG,"MUSIC output realloc failed %d",need); failed=true; break; }
          out=b; out_cap=need; continue;
        }
        if(derr!=ESP_AUDIO_ERR_OK){ ESP_LOGE(TAG,"MUSIC decode failed ret=%d",(int)derr); failed=true; break; }
        if(frame.decoded_size>0){
          esp_audio_simple_dec_info_t info={}; esp_audio_simple_dec_get_info(dec,&info);
          if(info.bits_per_sample!=16 || info.sample_rate==0 || info.channel==0){
            ESP_LOGE(TAG,"MUSIC unsupported PCM %uHz %ubit ch=%u",(unsigned)info.sample_rate,info.bits_per_sample,info.channel); failed=true; break;
          }
          if((int)info.sample_rate!=current_hz){
            current_hz=info.sample_rate; current_ch=info.channel;
            i2s_channel_disable(tx_chan); i2s_std_clk_config_t clk=I2S_STD_CLK_DEFAULT_CONFIG(current_hz);
            i2s_channel_reconfig_std_clock(tx_chan,&clk); i2s_channel_enable(tx_chan);
            ESP_LOGI(TAG,"MUSIC %s %dHz ch=%d",fmt.c_str(),current_hz,current_ch);
          }
          int samples=frame.decoded_size/(2*info.channel);
          if(samples>4096) samples=4096;
          const int16_t *pcm=(const int16_t*)frame.buffer;
          int vol=std::max(0,std::min(20,volume_));
          for(int i=0;i<samples;i++){
            int32_t mono=(info.channel>=2)?((int32_t)pcm[i*info.channel]+pcm[i*info.channel+1])/2:pcm[i];
            // Correct gain: volume 0..20, then PCM16 -> 32-bit I2S slot.
            int32_t scaled=(int32_t)(((int64_t)mono * vol) / 20);
            int32_t v=scaled << 16;
            stereo[i*2]=v; stereo[i*2+1]=v;
          }
          size_t wr=0; i2s_channel_write(tx_chan,stereo,samples*2*sizeof(int32_t),&wr,portMAX_DELAY);
        }
        if(raw.consumed<=0){ break; }
        raw.len-=raw.consumed; raw.buffer+=raw.consumed;
      }
      if(failed) break;
    }
    esp_audio_simple_dec_close(dec);
    esp_http_client_close(h); esp_http_client_cleanup(h);
    int32_t z[640]={0}; size_t wr=0; i2s_channel_write(tx_chan,z,sizeof(z),&wr,portMAX_DELAY);
    if(current_hz!=16000){ i2s_channel_disable(tx_chan); i2s_std_clk_config_t clk=I2S_STD_CLK_DEFAULT_CONFIG(16000); i2s_channel_reconfig_std_clock(tx_chan,&clk); i2s_channel_enable(tx_chan); }
    playing=false; set_led_(false); music_stop_=false;
    ESP_LOGI(TAG,"MUSIC DIRECT end format=%s%s",fmt.c_str(),failed?" ERROR":"");
  }
}

