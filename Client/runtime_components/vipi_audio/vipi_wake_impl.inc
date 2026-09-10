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

