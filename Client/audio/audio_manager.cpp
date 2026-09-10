#include "audio_manager.h"

#include "esp_log.h"

static const char *TAG_AUDIO_MANAGER = "vipi_audio_mgr";

ViPiAudioManager::ViPiAudioManager() = default;

ViPiAudioManager::~ViPiAudioManager() {
  if (mutex_ != nullptr) {
    vSemaphoreDelete(mutex_);
    mutex_ = nullptr;
  }
}

bool ViPiAudioManager::begin(i2s_chan_handle_t tx_channel) {
  tx_channel_ = tx_channel;

  if (mutex_ == nullptr) {
    mutex_ = xSemaphoreCreateRecursiveMutex();
  }

  if (mutex_ == nullptr) {
    ESP_LOGE(
        TAG_AUDIO_MANAGER,
        "Cannot create audio mutex");
    return false;
  }

  owner_ = Owner::NONE;
  owner_task_ = nullptr;
  lock_depth_ = 0;

  ESP_LOGI(
      TAG_AUDIO_MANAGER,
      "AudioManager ready");

  return tx_channel_ != nullptr;
}

bool ViPiAudioManager::acquire(
    Owner owner,
    TickType_t timeout) {
  if (mutex_ == nullptr) {
    return false;
  }

  if (xSemaphoreTakeRecursive(
          mutex_,
          timeout) != pdTRUE) {
    return false;
  }

  TaskHandle_t current = xTaskGetCurrentTaskHandle();

  if (owner_task_ == current) {
    ++lock_depth_;
  } else {
    owner_task_ = current;
    owner_ = owner;
    lock_depth_ = 1;
  }

  return true;
}

void ViPiAudioManager::release(Owner owner) {
  if (mutex_ == nullptr) {
    return;
  }

  TaskHandle_t current = xTaskGetCurrentTaskHandle();

  if (owner_task_ != current) {
    ESP_LOGW(
        TAG_AUDIO_MANAGER,
        "release from non-owner task");
    return;
  }

  if (owner_ != owner) {
    ESP_LOGW(
        TAG_AUDIO_MANAGER,
        "release owner mismatch");
  }

  if (lock_depth_ > 0) {
    --lock_depth_;
  }

  if (lock_depth_ == 0) {
    owner_ = Owner::NONE;
    owner_task_ = nullptr;
  }

  xSemaphoreGiveRecursive(mutex_);
}

bool ViPiAudioManager::write(
    Owner owner,
    const void *data,
    size_t bytes,
    size_t *written,
    TickType_t timeout) {
  if (tx_channel_ == nullptr) {
    return false;
  }

  if (owner_task_ != xTaskGetCurrentTaskHandle() ||
      owner_ != owner) {
    ESP_LOGE(
        TAG_AUDIO_MANAGER,
        "I2S write rejected: task does not own audio");
    return false;
  }

  return i2s_channel_write(
             tx_channel_,
             data,
             bytes,
             written,
             timeout) == ESP_OK;
}

bool ViPiAudioManager::busy() const {
  return owner_ != Owner::NONE;
}

ViPiAudioManager::Owner ViPiAudioManager::owner() const {
  return owner_;
}
