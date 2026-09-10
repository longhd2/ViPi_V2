#!/usr/bin/env python3

import json
import time
import threading
import paho.mqtt.client as mqtt

BROKER = "192.168.1.11"
PORT = 1883
USERNAME = "minhanh"
PASSWORD = "minhanh"

PREFIX = "vipi_test"
DISCOVERY = "homeassistant"

DEVICES = {
    # ---------- PHONG KHACH ----------
    "den_tran_phong_khach": {
        "domain": "light",
        "name": "Đèn trần phòng khách",
        "room": "Phòng khách",
    },
    "den_ban_phong_khach": {
        "domain": "light",
        "name": "Đèn bàn phòng khách",
        "room": "Phòng khách",
    },
    "o_cam_tivi": {
        "domain": "switch",
        "name": "Ổ cắm tivi",
        "room": "Phòng khách",
        "device_class": "outlet",
    },
    "quat_phong_khach": {
        "domain": "fan",
        "name": "Quạt phòng khách",
        "room": "Phòng khách",
    },
    "rem_cua_phong_khach": {
        "domain": "cover",
        "name": "Rèm cửa phòng khách",
        "room": "Phòng khách",
        "device_class": "curtain",
    },

    # ---------- PHONG NGU ----------
    "den_ngu": {
        "domain": "light",
        "name": "Đèn ngủ",
        "room": "Phòng ngủ",
    },
    "den_dau_giuong": {
        "domain": "light",
        "name": "Đèn đầu giường",
        "room": "Phòng ngủ",
    },
    "o_cam_dau_giuong": {
        "domain": "switch",
        "name": "Ổ cắm đầu giường",
        "room": "Phòng ngủ",
        "device_class": "outlet",
    },
    "dieu_hoa_phong_ngu": {
        "domain": "climate",
        "name": "Điều hòa phòng ngủ",
        "room": "Phòng ngủ",
    },

    # ---------- PHONG BEP ----------
    "den_bep": {
        "domain": "light",
        "name": "Đèn bếp",
        "room": "Phòng bếp",
    },
    "o_cam_bep": {
        "domain": "switch",
        "name": "Ổ cắm bếp",
        "room": "Phòng bếp",
        "device_class": "outlet",
    },
    "may_hut_mui": {
        "domain": "switch",
        "name": "Máy hút mùi",
        "room": "Phòng bếp",
    },
}

states = {}
temperatures = {"dieu_hoa_phong_ngu": 26.0}

client = mqtt.Client(
    mqtt.CallbackAPIVersion.VERSION2,
    client_id="vipi-hass-test-devices"
)

client.username_pw_set(USERNAME, PASSWORD)


def topic(key, suffix):
    return f"{PREFIX}/{key}/{suffix}"


def device_info(key, d):
    return {
        "identifiers": [f"vipi_test_{key}"],
        "name": d["name"],
        "manufacturer": "ViPi Team",
        "model": "ViPi MQTT Test Device",
    }


def publish_state(key):
    d = DEVICES[key]
    domain = d["domain"]

    if domain in ("light", "switch", "fan"):
        value = states.get(key, "OFF")
        client.publish(topic(key, "state"), value, retain=True)

    elif domain == "cover":
        value = states.get(key, "closed")
        client.publish(topic(key, "state"), value, retain=True)

    elif domain == "climate":
        value = states.get(key, "off")
        client.publish(topic(key, "mode_state"), value, retain=True)
        client.publish(
            topic(key, "temperature_state"),
            str(temperatures.get(key, 26.0)),
            retain=True,
        )
        client.publish(topic(key, "current_temperature"), "27.0", retain=True)


def discovery_config(key, d):
    domain = d["domain"]

    base = {
        "name": d["name"],
        "unique_id": f"vipi_test_{key}",
        "object_id": key,
        "device": device_info(key, d),
        "availability_topic": f"{PREFIX}/availability",
        "payload_available": "online",
        "payload_not_available": "offline",
    }

    if domain == "light":
        base.update({
            "command_topic": topic(key, "set"),
            "state_topic": topic(key, "state"),
            "payload_on": "ON",
            "payload_off": "OFF",
        })

    elif domain == "switch":
        base.update({
            "command_topic": topic(key, "set"),
            "state_topic": topic(key, "state"),
            "payload_on": "ON",
            "payload_off": "OFF",
        })
        if d.get("device_class"):
            base["device_class"] = d["device_class"]

    elif domain == "fan":
        base.update({
            "command_topic": topic(key, "set"),
            "state_topic": topic(key, "state"),
            "payload_on": "ON",
            "payload_off": "OFF",
        })

    elif domain == "cover":
        base.update({
            "command_topic": topic(key, "set"),
            "state_topic": topic(key, "state"),
            "payload_open": "OPEN",
            "payload_close": "CLOSE",
            "payload_stop": "STOP",
            "state_open": "open",
            "state_closed": "closed",
            "device_class": d.get("device_class", "curtain"),
        })

    elif domain == "climate":
        base.update({
            "mode_command_topic": topic(key, "mode_set"),
            "mode_state_topic": topic(key, "mode_state"),
            "modes": ["off", "cool"],
            "temperature_command_topic": topic(key, "temperature_set"),
            "temperature_state_topic": topic(key, "temperature_state"),
            "current_temperature_topic": topic(key, "current_temperature"),
            "min_temp": 16,
            "max_temp": 30,
            "temp_step": 1,
            "temperature_unit": "C",
        })

    return base


def publish_discovery():
    print("Publishing MQTT Discovery...")

    for key, d in DEVICES.items():
        cfg = discovery_config(key, d)

        config_topic = (
            f"{DISCOVERY}/{d['domain']}/"
            f"vipi_test_{key}/config"
        )

        client.publish(
            config_topic,
            json.dumps(cfg, ensure_ascii=False),
            retain=True,
        )

        if d["domain"] in ("light", "switch", "fan"):
            states.setdefault(key, "OFF")
        elif d["domain"] == "cover":
            states.setdefault(key, "closed")
        elif d["domain"] == "climate":
            states.setdefault(key, "off")

        publish_state(key)

    client.publish(
        f"{PREFIX}/availability",
        "online",
        retain=True,
    )

    print(f"Published {len(DEVICES)} devices")


def on_connect(client, userdata, flags, reason_code, properties):
    print("MQTT connected:", reason_code)

    client.subscribe(f"{PREFIX}/+/set")
    client.subscribe(f"{PREFIX}/+/mode_set")
    client.subscribe(f"{PREFIX}/+/temperature_set")

    publish_discovery()


def on_message(client, userdata, msg):
    parts = msg.topic.split("/")

    if len(parts) != 3:
        return

    _, key, command_type = parts

    if key not in DEVICES:
        return

    payload = msg.payload.decode("utf-8", errors="ignore").strip()
    d = DEVICES[key]
    domain = d["domain"]

    print(
        time.strftime("%H:%M:%S"),
        d["name"],
        command_type,
        payload,
    )

    if command_type == "set":
        if domain in ("light", "switch", "fan"):
            if payload.upper() == "ON":
                states[key] = "ON"
            elif payload.upper() == "OFF":
                states[key] = "OFF"

        elif domain == "cover":
            if payload.upper() == "OPEN":
                states[key] = "open"
            elif payload.upper() == "CLOSE":
                states[key] = "closed"
            elif payload.upper() == "STOP":
                pass

    elif command_type == "mode_set" and domain == "climate":
        if payload in ("off", "cool"):
            states[key] = payload

    elif command_type == "temperature_set" and domain == "climate":
        try:
            temperatures[key] = float(payload)
        except ValueError:
            return

    publish_state(key)


client.on_connect = on_connect
client.on_message = on_message

client.will_set(
    f"{PREFIX}/availability",
    "offline",
    retain=True,
)

print("====================================")
print(" ViPi HASS MQTT Test Devices")
print("====================================")
print("Broker :", BROKER)
print("Devices:", len(DEVICES))

client.connect(BROKER, PORT, 60)
client.loop_forever()
