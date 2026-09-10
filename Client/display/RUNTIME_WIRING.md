# Runtime wiring

The SSD1306 backend now writes a real 1024-byte framebuffer over I2C when
compiled for Arduino/ESP32.

Purple board defaults:

- SDA GPIO41
- SCL GPIO42
- address 0x3C
- 128x64
- I2C 400 kHz

The runtime should call:

- `display.set_state(...)` from StateManager transitions
- `display.set_wifi_strength(...)` from NetworkManager
- `display.set_muted(...)` from Audio/Mic mute state
- `display.show_volume(volume / max_volume)` after volume changes
- `display.set_timer(...)` from Timer state
- `display.set_transcript(...)` after STT
- `display.set_reply(...)` before/during TTS
- `display.update(millis())` from the main loop

The screen remains optional. Failure to initialize it must not stop ViPi.
