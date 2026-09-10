# ViPi V2 - Git / GitHub Guide

Repository: `longhd2/ViPi_V2`  
Visibility: **PRIVATE**

## Check changes

    clear
    cd /home/server/ViPi_V2
    git status --short
    git diff --stat

## Push development changes

After testing:

    clear
    cd /home/server/ViPi_V2
    git add -A
    git status --short
    git commit -m "Update ViPi"
    git push origin main

## Create v2.0.1 Stable

    clear
    cd /home/server/ViPi_V2
    printf '2.0.1\n' > VERSION
    git add -A
    git commit -m "ViPi V2.0.1"
    git push origin main
    git tag -a v2.0.1 -m "ViPi V2.0.1 Stable"
    git push origin v2.0.1

## Verify Local and GitHub

    clear
    cd /home/server/ViPi_V2
    echo "LOCAL  : $(git rev-parse --short HEAD)"
    echo "REMOTE : $(git ls-remote origin refs/heads/main | cut -c1-7)"
    git tag --sort=-version:refname

## Clone after Server failure

    clear
    cd /home/server
    gh repo clone longhd2/ViPi_V2 ViPi_V2
    cd ViPi_V2
    git status

## Files that must not be committed

    Client/.esphome/
    Client/*.bin
    Client/*.elf
    Server/cache/
    logs/
    backups/
    Server/config/secrets.yaml
    Server/data/clients.json
    Server/data/scheduler.json
    *.bak*

Workflow:

    Develop -> Test -> Diff -> Commit -> Push -> Stable Test -> Tag -> Push Tag

Never delete or move an old Stable tag.
