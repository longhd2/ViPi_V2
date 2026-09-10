#include "sound_manager.h"

#include <algorithm>
#include <cstring>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char *TAG = "vipi_sound";


static uint16_t read_u16_le(const uint8_t *p) {
  return static_cast<uint16_t>(
      p[0] |
      (static_cast<uint16_t>(p[1]) << 8)
  );
}


static uint32_t read_u32_le(const uint8_t *p) {
  return
      static_cast<uint32_t>(p[0]) |
      (static_cast<uint32_t>(p[1]) << 8) |
      (static_cast<uint32_t>(p[2]) << 16) |
      (static_cast<uint32_t>(p[3]) << 24);
}


bool ViPiSoundManager::read_(
    size_t offset,
    void *data,
    size_t length
) {
  if (storage_ == nullptr) {
    return false;
  }

  if (offset + length > storage_->size) {
    return false;
  }

  return esp_partition_read(
             storage_,
             offset,
             data,
             length
         ) == ESP_OK;
}


bool ViPiSoundManager::begin(
    i2s_chan_handle_t tx_channel
) {
  tx_channel_ = tx_channel;

  storage_ = esp_partition_find_first(
      ESP_PARTITION_TYPE_DATA,
      static_cast<esp_partition_subtype_t>(0x41),
      "storage"
  );

  if (storage_ == nullptr) {
    ESP_LOGE(TAG, "Khong tim thay partition storage");
    return false;
  }

  if (!read_(0, &header_, sizeof(header_))) {
    ESP_LOGE(TAG, "Khong doc duoc sound header");
    storage_ = nullptr;
    return false;
  }

  const char expected[8] = {
      'V', 'P', 'I', 'S',
      'N', 'D', '2', '\0'
  };

  if (memcmp(header_.magic, expected, 8) != 0) {
    ESP_LOGW(
        TAG,
        "Storage chua co ViPi Sound Pack"
    );
    storage_ = nullptr;
    return false;
  }

  if (header_.version != 2) {
    ESP_LOGE(
        TAG,
        "Sound Pack version khong ho tro: %u",
        static_cast<unsigned>(header_.version)
    );
    storage_ = nullptr;
    return false;
  }

  if (header_.entry_size != sizeof(PackEntry)) {
    ESP_LOGE(
        TAG,
        "Entry size sai: pack=%u firmware=%u",
        static_cast<unsigned>(header_.entry_size),
        static_cast<unsigned>(sizeof(PackEntry))
    );
    storage_ = nullptr;
    return false;
  }

  if (header_.total_size > storage_->size) {
    ESP_LOGE(TAG, "Sound Pack vuot partition");
    storage_ = nullptr;
    return false;
  }

  ESP_LOGI(
      TAG,
      "ViPi Sound Pack OK: %u sounds, %u bytes",
      static_cast<unsigned>(header_.count),
      static_cast<unsigned>(header_.total_size)
  );

  return true;
}


bool ViPiSoundManager::read_entry_(
    uint32_t index,
    PackEntry &entry
) {
  if (index >= header_.count) {
    return false;
  }

  const size_t offset =
      sizeof(PackHeader) +
      static_cast<size_t>(index) * header_.entry_size;

  return read_(
      offset,
      &entry,
      sizeof(entry)
  );
}


bool ViPiSoundManager::find_(
    const char *name,
    PackEntry &entry
) {
  if (storage_ == nullptr || name == nullptr) {
    return false;
  }

  for (uint32_t i = 0; i < header_.count; i++) {
    PackEntry candidate{};

    if (!read_entry_(i, candidate)) {
      return false;
    }

    candidate.name[sizeof(candidate.name) - 1] = '\0';

    if (strcmp(candidate.name, name) == 0) {
      entry = candidate;
      return true;
    }
  }

  return false;
}


bool ViPiSoundManager::has_sound(
    const char *name
) {
  PackEntry entry{};
  return find_(name, entry);
}


bool ViPiSoundManager::parse_wav_(
    const PackEntry &entry,
    WavInfo &wav
) {
  if (entry.size < 44) {
    return false;
  }

  uint8_t riff[12]{};

  if (!read_(entry.offset, riff, sizeof(riff))) {
    return false;
  }

  if (
      memcmp(riff, "RIFF", 4) != 0 ||
      memcmp(riff + 8, "WAVE", 4) != 0
  ) {
    ESP_LOGE(TAG, "%s khong phai WAV RIFF", entry.name);
    return false;
  }

  uint32_t pos = 12;

  bool have_fmt = false;
  bool have_data = false;

  while (pos + 8 <= entry.size) {
    uint8_t chunk_header[8]{};

    if (!read_(
            entry.offset + pos,
            chunk_header,
            sizeof(chunk_header)
        )) {
      return false;
    }

    const uint32_t chunk_size =
        read_u32_le(chunk_header + 4);

    const uint32_t chunk_data = pos + 8;

    if (
        chunk_data > entry.size ||
        chunk_size > entry.size - chunk_data
    ) {
      return false;
    }

    if (memcmp(chunk_header, "fmt ", 4) == 0) {
      uint8_t fmt[32]{};

      const size_t read_len =
          std::min<size_t>(
              sizeof(fmt),
              chunk_size
          );

      if (read_len < 16) {
        return false;
      }

      if (!read_(
              entry.offset + chunk_data,
              fmt,
              read_len
          )) {
        return false;
      }

      const uint16_t format =
          read_u16_le(fmt + 0);

      wav.channels =
          read_u16_le(fmt + 2);

      wav.sample_rate =
          read_u32_le(fmt + 4);

      wav.bits_per_sample =
          read_u16_le(fmt + 14);

      if (format != 1) {
        ESP_LOGE(
            TAG,
            "%s WAV khong phai PCM",
            entry.name
        );
        return false;
      }

      have_fmt = true;
    }

    if (memcmp(chunk_header, "data", 4) == 0) {
      wav.data_offset =
          entry.offset + chunk_data;

      wav.data_size = chunk_size;

      have_data = true;
    }

    if (have_fmt && have_data) {
      break;
    }

    pos =
        chunk_data +
        chunk_size +
        (chunk_size & 1U);
  }

  if (!have_fmt || !have_data) {
    return false;
  }

  if (
      wav.channels != 1 ||
      wav.sample_rate != 16000 ||
      wav.bits_per_sample != 16
  ) {
    ESP_LOGE(
        TAG,
        "%s can PCM16 mono 16000Hz, hien tai %uHz %ubit ch=%u",
        entry.name,
        static_cast<unsigned>(wav.sample_rate),
        static_cast<unsigned>(wav.bits_per_sample),
        static_cast<unsigned>(wav.channels)
    );
    return false;
  }

  return true;
}


bool ViPiSoundManager::play(
    const char *name,
    int volume
) {
  if (tx_channel_ == nullptr) {
    ESP_LOGE(TAG, "I2S TX chua san sang");
    return false;
  }

  PackEntry entry{};

  if (!find_(name, entry)) {
    ESP_LOGW(TAG, "Khong co sound: %s", name);
    return false;
  }

  WavInfo wav{};

  if (!parse_wav_(entry, wav)) {
    ESP_LOGE(TAG, "WAV invalid: %s", name);
    return false;
  }

  volume = std::max(0, std::min(20, volume));

  constexpr size_t SAMPLES = 512;

  int16_t mono[SAMPLES];
  int32_t stereo[SAMPLES * 2];

  uint32_t remaining = wav.data_size;
  uint32_t offset = wav.data_offset;

  ESP_LOGI(
      TAG,
      "PLAY local sound: %s size=%u volume=%d",
      name,
      static_cast<unsigned>(wav.data_size),
      volume
  );

  while (remaining > 0) {
    size_t bytes =
        std::min<size_t>(
            remaining,
            sizeof(mono)
        );

    bytes &= ~static_cast<size_t>(1);

    if (bytes == 0) {
      break;
    }

    if (!read_(offset, mono, bytes)) {
      ESP_LOGE(TAG, "Flash read failed: %s", name);
      return false;
    }

    const size_t samples =
        bytes / sizeof(int16_t);

    for (size_t i = 0; i < samples; i++) {
      const int32_t scaled =
          static_cast<int32_t>(
              (
                  static_cast<int64_t>(mono[i]) *
                  volume
              ) / 20
          );

      const int32_t sample32 =
          static_cast<int32_t>(
              static_cast<int64_t>(scaled) *
              65536LL
          );

      stereo[i * 2] = sample32;
      stereo[i * 2 + 1] = sample32;
    }

    size_t written = 0;

    const esp_err_t err =
        i2s_channel_write(
            tx_channel_,
            stereo,
            samples * 2 * sizeof(int32_t),
            &written,
            portMAX_DELAY
        );

    if (err != ESP_OK) {
      ESP_LOGE(
          TAG,
          "I2S write failed: %s",
          esp_err_to_name(err)
      );
      return false;
    }

    offset += bytes;
    remaining -= bytes;
  }

  // Them mot doan silence ngan de ket thuc sach.
  int32_t silence[128 * 2]{};
  size_t written = 0;

  i2s_channel_write(
      tx_channel_,
      silence,
      sizeof(silence),
      &written,
      portMAX_DELAY
  );

  ESP_LOGI(TAG, "DONE local sound: %s", name);

  return true;
}
