#pragma once

#include <cstddef>
#include <cstdint>

#include "driver/i2s_std.h"
#include "esp_partition.h"


class ViPiSoundManager {
 public:
  bool begin(i2s_chan_handle_t tx_channel);

  bool available() const {
    return storage_ != nullptr;
  }

  bool has_sound(const char *name);

  bool play(
      const char *name,
      int volume = 14
  );

 private:
  struct PackHeader {
    char magic[8];
    uint32_t version;
    uint32_t count;
    uint32_t entry_size;
    uint32_t data_start;
    uint32_t total_size;
    uint32_t reserved;
  };

  struct PackEntry {
    char name[64];
    uint32_t offset;
    uint32_t size;
    uint32_t flags;
    uint32_t reserved;
  };

  struct WavInfo {
    uint32_t data_offset = 0;
    uint32_t data_size = 0;
    uint32_t sample_rate = 0;
    uint16_t channels = 0;
    uint16_t bits_per_sample = 0;
  };

  bool read_entry_(
      uint32_t index,
      PackEntry &entry
  );

  bool find_(
      const char *name,
      PackEntry &entry
  );

  bool parse_wav_(
      const PackEntry &entry,
      WavInfo &wav
  );

  bool read_(
      size_t offset,
      void *data,
      size_t length
  );

  const esp_partition_t *storage_ = nullptr;
  i2s_chan_handle_t tx_channel_ = nullptr;
  PackHeader header_{};
};
