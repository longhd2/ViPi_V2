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
  if (nvs_get_i32(h,"threshold",&i)==ESP_OK) threshold_=std::max(0.05f,std::min(0.95f,i/100.0f));
  if (nvs_get_i32(h,"record_ms",&i)==ESP_OK) wake_record_ms_=std::max(1000,std::min(30000,(int)i));
  if (nvs_get_i32(h,"volume",&i)==ESP_OK) volume_=std::max(1,std::min(20,(int)i));
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
  if (e==ESP_OK) e=nvs_commit(h);
  nvs_close(h);
  return e==ESP_OK;
}

