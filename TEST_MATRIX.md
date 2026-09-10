# ViPi V2 Test Matrix

| Hạng mục | Host test | Hardware test sau khi nạp ESP32 |
|---|---|---|
| No LLM/Ollama/Qwen | Có | Không cần |
| Normalize + intent + fuzzy | Có | Không cần |
| Room context | Có | Không cần |
| Skill registry isolation | Có | Không cần |
| HA fail graceful | Có | Có |
| Media command routing | Có | Có |
| Timer routing | Có | Có |
| Audio priority MEDIA/TTS/SYSTEM/ALARM | Có | Có |
| TTS gain không shift bit | Có | Có nghe thực tế |
| LED count động/volume 24 LED | Có | Có |
| LED state mapping | Có | Có |
| 4 touch short/long/repeat logic | Có logic | Có cảm ứng thực tế |
| GPIO conflict validator | Có | Có |
| I2S speaker/mic driver | Chưa thể host-test | Bắt buộc |
| Opus TCP TTS thực tế | Chưa tích hợp V1 runtime | Bắt buộc |
| M4A/AAC media path V1 | Chưa tích hợp vào V2 source này | Bắt buộc |
| Web/NVS/OTA thực tế | Spec/module boundary | Bắt buộc |

Không gọi các phần phụ thuộc phần cứng là PASS trước khi compile/nạp trên ESP32.
