# ViPi V2.0 - Recovery Guide

Stable release: **2.0.3**

## What is stored in Git

- ViPi Server source
- ViPi ESP32 Client source
- Current runtime component source
- Understanding Engine and Skill system
- Home Assistant skill/service
- Media, Timer, Reminder, Weather and Utility skills
- Web Admin
- Supervisor configuration
- System sounds
- Python requirements
- Configuration templates

## Intentionally NOT stored

The following files are generated, private or runtime-specific:

- Client/.esphome
- compiled .bin/.elf firmware
- Server/cache
- logs
- backups
- Python virtual environments
- Server/config/secrets.yaml
- runtime clients.json
- runtime scheduler.json

## Restore outline

1. Clone the repository.
2. Install Debian system dependencies.
3. Create Python environment.
4. Install requirements.txt.
5. Copy Server/config/secrets.example.yaml to secrets.yaml.
6. Fill in private credentials locally.
7. Check Server/config/vipi.yaml.
8. Install/configure ESPHome environment.
9. Build Client firmware with Client/build.sh.
10. Start ViPi with supervisor/supervisord.conf.

Never commit secrets.yaml.
