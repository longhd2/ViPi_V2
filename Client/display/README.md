# ViPi V2 Display

ViPi V2 supports only two display families to keep the firmware maintainable.

## 1. SSD1306 128x64 I2C

This is the default display for the current purple ViPi board.

Default wiring:

- SDA: GPIO41
- SCL: GPIO42
- Resolution: 128x64
- Mirror X: enabled
- Mirror Y: enabled
- I2C address: 0x3C

## 2. ST7789 SPI

Optional color display for future ViPi hardware.

Pins are not hard-coded. MOSI, CLK, DC, RST, CS and Backlight are configured
from the Web interface and stored in NVS.

## Web selection

Display type:

- None
- OLED SSD1306 128x64 I2C
- ST7789 SPI

Selecting SSD1306 shows only I2C settings.
Selecting ST7789 shows only SPI settings.
Selecting None disables the display module.

The display follows the central ViPi device state and must never be required
for wake, audio, media or network operation.


## Driver architecture

`DisplayManager -> DisplayRenderer -> DisplayDriver -> SSD1306/ST7789`

Icons are built into `display_icons.h`, so the basic status UI does not depend
on external PNG files.

The UI is state-driven. Audio, wake, touch and network modules should publish
state/data to DisplayManager instead of drawing directly to the LCD.
