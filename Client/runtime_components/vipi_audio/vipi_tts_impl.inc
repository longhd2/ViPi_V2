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
    int c=accept(ls,nullptr,nullptr); if (c<0) continue;
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
            int64_t v=((int64_t)pcm[i]) << volume;
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
    close(c);
  }
}


