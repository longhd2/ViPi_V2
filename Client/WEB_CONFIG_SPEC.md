# Web Config V2
Device: device_id, room, server_ip.

Mic: WS/BCLK/DIN. Speaker: BCLK/WS/DOUT/AMP enable.

LED: type, pin, count, RGB order, brightness, reverse, offset, animation speed.

Touch 1..4: GPIO, threshold, debounce, long press, repeat, short action, long action.

Audio: volume, wake sound, button sound.

Actions: Test LED, Test Speaker, Test Touch, Save, Reboot, Factory Reset, OTA.

GPIO validator must reject conflicts and unsafe/reserved pins for the chosen board profile.



## DISPLAY

### Loại màn hình

- Không sử dụng màn hình
- OLED SSD1306 128x64 I2C
- ST7789 SPI

### OLED SSD1306

Default profile for the purple board:

- Width: 128
- Height: 64
- SDA: GPIO41
- SCL: GPIO42
- Mirror X: ON
- Mirror Y: ON
- Address: 0x3C

### ST7789

Web-configurable:

- Width / Height
- Rotation
- MOSI
- CLK
- DC
- RST
- CS
- Backlight
- Brightness

Display GPIO settings must be checked by the shared GPIO conflict validator
before being saved. Settings are stored in NVS and loaded on reboot.
