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
  std::string page=R"HTML(<!doctype html><html><head><meta name=viewport content="width=device-width,initial-scale=1"><meta charset=utf-8><title>ViPi V2</title><style>body{font-family:system-ui;margin:0;background:#f5f5f7;color:#222}.w{max-width:620px;margin:auto;padding:22px}.c{background:white;border-radius:18px;padding:20px;margin:14px 0;box-shadow:0 2px 12px #0001}h2{margin:0 0 12px;color:#6536a6}label{display:block;margin:12px 0 5px;font-weight:600}input{box-sizing:border-box;width:100%;padding:12px;border:1px solid #bbb;border-radius:10px;font-size:16px}button{padding:12px 16px;border:0;border-radius:10px;background:#6536a6;color:white;font-weight:700;font-size:15px;margin:8px 6px 0 0}.muted{color:#666;font-size:14px}.ok{color:#087f23}</style></head><body><div class=w><div class=c><h2>ViPi V2</h2><div class=ok>● Đang hoạt động</div><div class=muted>Wi‑Fi đã kết nối. Wi‑Fi được đổi qua ViPi-Setup khi mất kết nối.</div></div><form class=c method=post action=/save><label>Tên phòng / thiết bị</label><input name=room value=")HTML";
  page+=html_escape(self->room_); page+=R"HTML("><label>Máy chủ ViPi</label><input name=server value=")HTML";
  page+=html_escape(self->server_ip_); page+=R"HTML(" inputmode=decimal><button type=button onclick="findServer()">Tìm máy chủ</button><div id=find class=muted></div><label>Hotword MultiNet</label><input name=wake value=")HTML";
  page+=html_escape(self->command_); page+=R"HTML(" placeholder="V B"><label>Ngưỡng nhận (0.05–0.95)</label><input name=threshold type=number min=.05 max=.95 step=.01 value=")HTML";
  char b[32]; snprintf(b,sizeof(b),"%.2f",self->threshold_); page+=b; page+=R"HTML("><label>Thời gian nghe sau wake (ms)</label><input name=record_ms type=number min=1000 max=30000 step=500 value=")HTML";
  page+=std::to_string(self->wake_record_ms_); page+=R"HTML("><label>Âm lượng mặc định (1–20)</label><input name=volume type=number min=1 max=20 value=")HTML";
  page+=std::to_string(self->volume_); page+=R"HTML("><br><button type=submit>Lưu cấu hình</button></form><div class=c><b>Cập nhật firmware OTA</b><p class=muted>Chọn <b>VIPI_V2_CLIENT_APP.bin</b>. Không dùng FULL.bin ở đây.</p><input id=fw type=file accept=.bin><button type=button onclick="ota()">Cập nhật OTA</button><div id=ota class=muted></div></div><div class=c><b>Wi‑Fi lần đầu / đổi router</b><p class=muted>Nếu không kết nối được Wi‑Fi, ViPi phát điểm truy cập <b>ViPi-Setup</b>. Kết nối bằng mật khẩu <b>12345678</b>, sau đó mở <b>192.168.4.1</b> để nhập Wi‑Fi mới.</p></div></div><script>async function findServer(){let e=document.getElementById('find');e.textContent='Đang tìm...';try{let r=await fetch('/find');let j=await r.json();if(j.found){document.querySelector('[name=server]').value=j.ip;e.textContent='Đã tìm thấy '+j.ip}else e.textContent='Chưa thấy server. Có thể nhập IP thủ công.'}catch(x){e.textContent='Không tìm được.'}}async function ota(){let f=document.getElementById('fw').files[0],e=document.getElementById('ota');if(!f){e.textContent='Hãy chọn file APP.bin';return}if(!f.name.endsWith('.bin')){e.textContent='File không hợp lệ';return}if(!confirm('Cập nhật '+f.name+'? Không tắt nguồn trong khi cập nhật.'))return;e.textContent='Đang tải firmware...';try{let r=await fetch('/ota',{method:'POST',headers:{'Content-Type':'application/octet-stream','X-Filename':f.name},body:f});e.textContent=await r.text();if(r.ok)e.textContent+=' Thiết bị sẽ khởi động lại.'}catch(x){e.textContent='Mất kết nối khi reboot có thể là bình thường.'}}</script></body></html>)HTML";
  httpd_resp_set_type(req,"text/html; charset=utf-8");
  return httpd_resp_send(req,page.c_str(),page.size());
}

esp_err_t ViPiAudio::web_save_(httpd_req_t *req) {
  auto *self=(ViPiAudio*)req->user_ctx;
  std::string body; body.resize(req->content_len);
  int got=0; while(got<req->content_len){int r=httpd_req_recv(req,&body[got],req->content_len-got);if(r<=0)break;got+=r;} body.resize(got);
  std::string v;
  v=form_value_(body,"server"); if(!v.empty()){ in_addr a{}; if(inet_pton(AF_INET,v.c_str(),&a)==1) self->server_ip_=v; }
  v=form_value_(body,"wake"); if(!v.empty() && v.size()<=63) self->command_=v;
  v=form_value_(body,"room"); if(!v.empty() && v.size()<=63) self->room_=v;
  v=form_value_(body,"threshold"); if(!v.empty()) self->threshold_=std::max(0.05f,std::min(0.95f,(float)atof(v.c_str())));
  v=form_value_(body,"record_ms"); if(!v.empty()) self->wake_record_ms_=std::max(1000,std::min(30000,atoi(v.c_str())));
  v=form_value_(body,"volume"); if(!v.empty()) self->volume_=std::max(1,std::min(20,atoi(v.c_str())));
  bool ok=self->save_settings_(); self->config_dirty_=true;
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
  // Raw application binary upload. The browser sends the selected APP.bin
  // directly as application/octet-stream, so no multipart parser is needed.
  if (req->content_len <= 0 || req->content_len > 0x280000) {
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


