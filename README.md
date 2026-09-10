# ViPi V2

Vietnamese Private Voice Assistant

**Version:** 2.0.0.1  
**Server:** Debian / Python  
**Client:** ESP32-S3 / ESPHome  
**Architecture:** Local-first, Skill-based, Multi-device, No LLM

## Architecture

ESP32 -> STT/VAD -> Understanding Engine -> Intent/Slots -> Skill Registry -> TTS -> ESP32

Understanding Engine:
- Normalize
- Pattern matching
- Alias / synonym
- Fuzzy matching
- Context
- Multi-intent
- Confidence
- Exclusive intents

ViPi V2 does not use an LLM. Home Assistant is one independent Skill, not the core of ViPi.

## Main Skills

- Home Assistant
- Media
- Timer / Alarm
- Reminder
- Scheduled Action
- Weather
- Utility
- Calculator
- Location
- News
- System

## Multi-intent

Example:

    mấy giờ rồi và phát nhạc Nơi Này Có Anh

Plan:

    utility.time
    media.play

Media playback is deferred until TTS responses are completed.

## Home Assistant

Entity resolution priority:

    friendly_name -> aliases -> Area -> entity_id

Designed domains:

    light
    switch
    fan
    climate
    cover
    script
    scene
    automation
    sensor
    binary_sensor

Home Assistant failure does not stop independent ViPi functions such as STT, TTS, Media, Timer, Reminder, Weather and Utility.

## Network

| Port | Function |
|---:|---|
| UDP 3000 | Microphone audio |
| TCP 3001 | TTS / Speaker |
| TCP 3002 | Control |
| TCP 8088 | Media Proxy |
| TCP 8090 | Web Admin |
| TCP 8100 | Local STT |

Web Admin:

    http://192.168.1.222:8090

Do not close TCP 3001 while ViPi is running.

## Build ESP32

See RUN_GUIDE.md.

## Documentation

- README.md - architecture and features
- INSTALL.md - installation/recovery
- RUN_GUIDE.md - operating commands
- GIT_GUIDE.md - Git/GitHub/version workflow
- VERSION - current version

## GitHub

Repository: `longhd2/ViPi_V2`  
Visibility: **PRIVATE**

Versions:

    v2.0.0     Initial Stable
    v2.0.0.1   Documentation Release

## Security

Never commit secrets.yaml, Home Assistant tokens, MQTT passwords, API keys, runtime data, caches, ESPHome build directories, .bin or .elf files.

**ViPi V2.0.0.1**
