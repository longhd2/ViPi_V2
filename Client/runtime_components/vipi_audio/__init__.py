import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.components import esp32

CODEOWNERS = []
DEPENDENCIES = ["esp32", "wifi"]

ns = cg.esphome_ns.namespace("vipi_audio")
ViPiAudio = ns.class_("ViPiAudio", cg.Component)

CONF_SERVER_IP = "server_ip"
CONF_COMMAND = "command"
CONF_THRESHOLD = "threshold"
CONF_WAKE_RECORD_MS = "wake_record_ms"
CONF_MIC_WS_PIN = "mic_ws_pin"
CONF_MIC_BCLK_PIN = "mic_bclk_pin"
CONF_MIC_DIN_PIN = "mic_din_pin"
CONF_SPK_BCLK_PIN = "spk_bclk_pin"
CONF_SPK_WS_PIN = "spk_ws_pin"
CONF_SPK_DOUT_PIN = "spk_dout_pin"
CONF_PTT_PIN = "ptt_pin"
CONF_LED_PIN = "led_pin"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(ViPiAudio),
    cv.Required(CONF_SERVER_IP): cv.string,
    cv.Optional(CONF_COMMAND, default="V P"): cv.string,
    cv.Optional(CONF_THRESHOLD, default=0.20): cv.float_range(min=0.01, max=0.99),
    cv.Optional(CONF_WAKE_RECORD_MS, default=6000): cv.int_range(min=1000, max=30000),
    cv.Optional(CONF_MIC_WS_PIN, default=4): cv.int_range(min=0, max=48),
    cv.Optional(CONF_MIC_BCLK_PIN, default=5): cv.int_range(min=0, max=48),
    cv.Optional(CONF_MIC_DIN_PIN, default=6): cv.int_range(min=0, max=48),
    cv.Optional(CONF_SPK_BCLK_PIN, default=15): cv.int_range(min=0, max=48),
    cv.Optional(CONF_SPK_WS_PIN, default=16): cv.int_range(min=0, max=48),
    cv.Optional(CONF_SPK_DOUT_PIN, default=7): cv.int_range(min=0, max=48),
    cv.Optional(CONF_PTT_PIN, default=39): cv.int_range(min=-1, max=48),
    cv.Optional(CONF_LED_PIN, default=48): cv.int_range(min=0, max=48),
}).extend(cv.COMPONENT_SCHEMA)

async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_server_ip(config[CONF_SERVER_IP]))
    cg.add(var.set_command(config[CONF_COMMAND]))
    cg.add(var.set_threshold(config[CONF_THRESHOLD]))
    cg.add(var.set_wake_record_ms(config[CONF_WAKE_RECORD_MS]))
    cg.add(var.set_pins(
        config[CONF_MIC_WS_PIN], config[CONF_MIC_BCLK_PIN], config[CONF_MIC_DIN_PIN],
        config[CONF_SPK_BCLK_PIN], config[CONF_SPK_WS_PIN], config[CONF_SPK_DOUT_PIN],
        config[CONF_PTT_PIN], config[CONF_LED_PIN]
    ))

    esp32.add_idf_component(
        name="esp-sr",
        repo="https://github.com/espressif/esp-sr.git",
        ref="master",
    )
    esp32.add_idf_component(
        name="esp-libopus",
        repo="https://github.com/kahrendt/esp-libopus.git",
        ref="main",
    )
    # Official Espressif codec library: MP3 + AAC + M4A container parsing.
    esp32.add_idf_component(
        name="esp_audio_codec",
        repo="https://github.com/espressif/esp-adf-libs.git",
        ref="master",
        path="esp_audio_codec",
    )
