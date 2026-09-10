#pragma once

#include <stdint.h>

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class ViPiAudioManager {
 public:
  enum class Owner : uint8_t {
    NONE = 0,
    MEDIA = 1,
    TTS = 2,
    SYSTEM = 3,
    ALARM = 4
  };

  ViPiAudioManager();
  ~ViPiAudioManager();

  bool begin(i2s_chan_handle_t tx_channel);

  bool acquire(
      Owner owner,
      TickType_t timeout = portMAX_DELAY);

  void release(Owner owner);

  bool write(
      Owner owner,
      const void *data,
      size_t bytes,
      size_t *written,
      TickType_t timeout = portMAX_DELAY);

  bool busy() const;
  Owner owner() const;

  i2s_chan_handle_t channel() const {
    return tx_channel_;
  }

 private:
  SemaphoreHandle_t mutex_{nullptr};
  i2s_chan_handle_t tx_channel_{nullptr};

  volatile Owner owner_{Owner::NONE};
  TaskHandle_t owner_task_{nullptr};
  uint32_t lock_depth_{0};
};
