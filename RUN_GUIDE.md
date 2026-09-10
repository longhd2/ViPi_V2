# ViPi V2 - Run Guide

## Check version

    clear
    cd /home/server/ViPi_V2
    cat VERSION
    git log -1 --oneline
    git describe --tags --always

## Check Server

    clear
    ps aux | grep '[p]ython.*Server/main.py'

## Check ports

    clear
    ss -lntup | grep -E ':3000|:3001|:3002|:8088|:8090|:8100'

## Run Server manually

    clear
    cd /home/server/ViPi_V2
    /home/server/onju-v2/env/bin/python -m Server.main

If Supervisor already manages ViPi, do not start a second Server instance.

## Build ESP32 Client

    clear
    cd /home/server/ViPi_V2/Client
    ./build.sh

## Test Understanding / HASS

    clear
    cd /home/server/ViPi_V2
    /home/server/onju-v2/env/bin/python Server/tools/test_voice.py

## Check Git

    clear
    cd /home/server/ViPi_V2
    git status --short --branch
    git log --oneline --decorate -10
    git tag --sort=-version:refname

## Web Admin

    http://192.168.1.222:8090
