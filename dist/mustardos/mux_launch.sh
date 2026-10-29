#!/bin/sh
# HELP: HI5: a five-voice FM glitch beat generator. MENU quits.
# ICON: hi5
# GRID: HI5

# MustardOS starts an app with this script, passing the app's folder. It sets up
# the app the way MustardOS's own apps do, then runs launch.sh, which is shared
# with the TrimUI OS package. The app sees this file and so updates itself from
# the MustardOS package.

APP_DIR="${1:-$(dirname "$0")}"

if [ -f /opt/muos/script/var/retro.sh ]; then
    . /opt/muos/script/var/retro.sh
    command -v SETUP_APP >/dev/null 2>&1 && SETUP_APP hi5 ""
elif [ -f /opt/muos/script/var/func.sh ]; then
    # older muOS releases
    . /opt/muos/script/var/func.sh
    echo app >/tmp/act_go
    command -v SETUP_SDL_ENVIRONMENT >/dev/null 2>&1 && SETUP_SDL_ENVIRONMENT
    command -v SET_VAR >/dev/null 2>&1 && SET_VAR "system" "foreground_process" "hi5"
fi

cd "$APP_DIR" || exit 1
# the screen can stay dark once the app's window opens: the app runs this then
export HI5_SCREEN_WAKE="$PWD/wake_screen.sh"
./launch.sh
