# ESPHome display integration

The ViPi display layer now has:

- DisplayManager: state and overlay controller
- DisplayRenderer: decides what content/icon to draw
- DisplayDriver: common drawing API
- Ssd1306Driver: SSD1306 validation/backend
- St7789Driver: ST7789 validation/backend
- display_icons.h: built-in monochrome icons

## Important hardware binding

`draw_text`, `draw_icon`, `clear`, `present` and `draw_progress` are intentionally
the hardware boundary. They must be bound to the ESPHome display canvas /
controller object in the final ESPHome component build.

This separation prevents state/UI code from depending on a specific ESPHome
display implementation and keeps SSD1306/ST7789 selectable from Web/NVS.

## SSD1306 default

Purple board:

- 128x64
- I2C
- SDA GPIO41
- SCL GPIO42
- address 0x3C

## UI

128x64 layout supports:

- Wi-Fi icon
- mute icon
- wake screen
- listening icon/text
- thinking screen
- speaking/reply text
- music icon
- timer countdown/progress
- warning/error
- OTA
- boot/connect
- short STT text
- volume overlay

Vietnamese text should use a font with the required glyphs when the final
ESPHome font asset is selected. The current compact fallback labels are ASCII
to remain safe on minimal embedded fonts.
