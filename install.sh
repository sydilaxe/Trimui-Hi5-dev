#!/usr/bin/env bash
# Install or update HI5 on a TrimUI Brick, Brick Hammer or Brick Pro, running TrimUI OS or
# MustardOS. Run it on a computer on the same network as the TrimUI:
#
#   curl -fsSL https://raw.githubusercontent.com/sydilaxe/Trimui-Hi5-dev/main/install.sh | bash
#
# It finds the TrimUI over SSH (the firmware's SSH server must be on), compares the installed
# version with the latest GitHub release, and copies the new build there if the release is newer:
# to SD:/Apps/HI5 on TrimUI OS, or to MustardOS's application folder (MUOS/application/HI5) on
# MustardOS, which it tells apart by itself. Your song (hi5.sav) and any samples you added stay as
# they are.
#
#   HI5_HOST=192.168.1.28    skip the scan and use this address
#   HI5_USER=root            SSH user (root by default)
#   HI5_DEVICE=brickpro      the Brick Pro's build (with the sticks); brick for the Brick and Brick
#                           Hammer. MustardOS and an installed copy say which it is, so this is only
#                           needed the first time on TrimUI OS.
#   HI5_FORCE=1              reinstall even if the TrimUI already has the latest version
#
# The address that worked is saved in ~/.config/hi5-trimui/host and tried first next time.

set -euo pipefail

REPO=sydilaxe/Trimui-Hi5-dev
APPS=/mnt/SDCARD/Apps                       # TrimUI OS
MUX_APPS=/run/muos/storage/application     # MustardOS: SD2's or SD1's MUOS/application
SSH_USER=${HI5_USER:-root}
SAVED=${XDG_CONFIG_HOME:-$HOME/.config}/hi5-trimui/host

say() { printf 'hi5: %s\n' "$*" >&2; }
die() { say "$*"; exit 1; }

# print "IP BANNER" if IP answers on port 22 with an SSH banner
probe() {
    local banner=""
    { exec 3<>"/dev/tcp/$1/22"; } 2>/dev/null || return 0
    IFS= read -r -t 2 banner <&3 || true
    exec 3<&-
    [[ $banner == SSH-* ]] && printf '%s %s\n' "$1" "${banner%$'\r'}"
    return 0
}

# probe one address, giving up after 3 s (a host on another network can take minutes to time out)
probe_quick() {
    local i pid
    probe "$1" >"$tmp/one" &
    pid=$!
    for i in $(seq 30); do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.1
    done
    { kill "$pid" || true; wait "$pid" || true; } 2>/dev/null
    cat "$tmp/one"
}

local_ip() {
    if command -v ip >/dev/null; then
        ip -4 route get 1.1.1.1 2>/dev/null | sed -n 's/.* src \([0-9.]*\).*/\1/p'
    else
        local ifc
        ifc=$(route -n get default 2>/dev/null | awk '/interface:/ {print $2}')
        [ -n "$ifc" ] && ipconfig getifaddr "$ifc"
    fi
}

# probe every address on this machine's /24 at once; the handhelds' sshd is usually dropbear,
# which sets them apart from the OpenSSH hosts on a typical home network (any SSH host is
# offered when no dropbear answers)
scan() {
    local me prefix i line found=()
    me=$(local_ip || true)
    [ -n "$me" ] || die "can't work out this computer's LAN address; set HI5_HOST=<trimui ip>"
    prefix=${me%.*}
    say "looking for the TrimUI on $prefix.0/24..."
    for i in $(seq 1 254); do
        [ "$prefix.$i" = "$me" ] || probe "$prefix.$i" &
    done >"$tmp/scan"
    sleep 3
    { kill $(jobs -p) || true; wait; } 2>/dev/null
    while read -r line; do found+=("$line"); done < <(grep -i dropbear "$tmp/scan" | sort -t. -k4 -n)
    if [ ${#found[@]} -eq 0 ]; then
        while read -r line; do found+=("$line"); done < <(sort -t. -k4 -n "$tmp/scan")
    fi

    if [ ${#found[@]} -eq 0 ]; then
        say "no SSH server answered on $prefix.0/24."
        say "turn on SSH on the TrimUI, or set HI5_HOST=<trimui ip>"
        exit 1
    elif [ ${#found[@]} -eq 1 ]; then
        host=${found[0]%% *}
    else
        say "more than one candidate:"
        for i in "${!found[@]}"; do say "  $((i + 1))) ${found[$i]}"; done
        read -r -p "hi5: which one? " i </dev/tty
        [[ $i =~ ^[0-9]+$ ]] && [ "$i" -ge 1 ] && [ "$i" -le ${#found[@]} ] || die "no such choice"
        host=${found[$((i - 1))]%% *}
    fi
}

# true if dotted version $1 is newer than $2
newer() {
    awk -v a="$1" -v b="$2" 'BEGIN {
        n = split(a, x, "."); m = split(b, y, ".")
        for (i = 1; i <= (n > m ? n : m); i++) {
            if (x[i] + 0 > y[i] + 0) exit 0
            if (x[i] + 0 < y[i] + 0) exit 1
        }
        exit 1 }'
}

on_device() {
    ssh -o ControlMaster=auto -o ControlPath="$tmp/ssh" -o ControlPersist=120 \
        -o StrictHostKeyChecking=accept-new -o HostKeyAlgorithms=+ssh-rsa \
        -o ConnectTimeout=5 "$SSH_USER@$host" "$@"
}

main() {
    local c latest installed device asset os folder dest where

    for c in curl unzip ssh tar awk; do
        command -v "$c" >/dev/null || die "this needs $c"
    done

    tmp=$(mktemp -d)
    trap 'ssh -o ControlPath="$tmp/ssh" -O exit trimui 2>/dev/null || true; rm -rf "$tmp"' EXIT

    latest=$(curl -fsSL "https://api.github.com/repos/$REPO/releases/latest" |
        sed -n 's/.*"tag_name": *"v\{0,1\}\([^"]*\)".*/\1/p' | head -n 1) || true
    [ -n "$latest" ] || die "couldn't read the latest release from github.com/$REPO"
    say "latest release: v$latest"

    host=${HI5_HOST:-}
    if [ -z "$host" ] && [ -s "$SAVED" ]; then
        host=$(cat "$SAVED")
        if [ -n "$(probe_quick "$host")" ]; then
            say "using the saved address $host"
        else
            say "saved address $host isn't answering"
            host=
        fi
    fi
    [ -n "$host" ] || scan

    say "connecting to $SSH_USER@$host"
    # "OS VERSION DEVICE": the firmware (trimui / mustardos), and the installed version and build
    # (brick / brickpro), each "-" when not known. MustardOS names the device itself.
    installed=$(on_device "if [ -d /opt/muos ]; then
            d='$MUX_APPS/HI5'; os=mustardos
            case \$(cat /opt/muos/device/config/board/name 2>/dev/null) in
            tui-brick-pro) dev=brickpro ;; tui-brick) dev=brick ;; *) dev= ;; esac
        else
            test -d $APPS || exit 3; d=$APPS/HI5; os=trimui; dev=
        fi
        v=\$(cat \"\$d/VERSION\" 2>/dev/null); dev=\${dev:-\$(cat \"\$d/DEVICE\" 2>/dev/null)}
        echo \"\$os \${v:--} \${dev:--}\"" </dev/null) ||
        die "couldn't log in to $host, or it has no $APPS (is it the TrimUI card?)"
    mkdir -p "$(dirname "$SAVED")" && echo "$host" >"$SAVED"
    read -r os installed device <<<"$(printf '%s\n' "$installed" | head -n 1)"
    [ "$installed" = - ] && installed=
    [ "$device" = - ] && device=
    device=${HI5_DEVICE:-$device}
    case $device in
    brick | brickpro) ;;
    "")
        device=brick
        say "installing the Brick / Brick Hammer build (HI5_DEVICE=brickpro for the Brick Pro's)"
        ;;
    *) die "HI5_DEVICE is brick or brickpro, not $device" ;;
    esac
    if [ "$os" = mustardos ]; then
        asset=HI5-mustardos-$device.muxapp folder="HI5" dest=$MUX_APPS where=Applications
        say "MustardOS on a $([ $device = brickpro ] && echo "Brick Pro" || echo "Brick / Brick Hammer")"
    else
        asset=HI5-$device.zip folder=HI5 dest=$APPS where=Apps
    fi

    if [ -z "$installed" ]; then
        say "the TrimUI has no HI5 yet"
    elif [ "${HI5_FORCE:-}" != 1 ] && ! newer "$latest" "$installed"; then
        say "the TrimUI already has v$installed, nothing to do (HI5_FORCE=1 reinstalls)"
        return
    else
        say "the TrimUI has v$installed"
    fi

    if on_device "pidof hi5" </dev/null >/dev/null; then
        die "HI5 is running on the TrimUI; quit it (MENU) and run this again"
    fi

    say "downloading v$latest ($asset)..."
    curl -fsSL -o "$tmp/$asset" "https://github.com/$REPO/releases/download/v$latest/$asset"
    unzip -q "$tmp/$asset" -d "$tmp/zip"
    # the TrimUI OS zip holds Apps/HI5, the .muxapp "HI5"
    local src="$tmp/zip/Apps"
    [ "$os" = mustardos ] && src="$tmp/zip"
    [ -f "$src/$folder/hi5" ] || die "the release's $asset has no $folder/hi5"
    # the song is the user's (releases don't carry one, but never overwrite it)
    rm -f "$src/$folder/"hi5.sav*

    say "copying to $host:$dest/$folder..."
    (cd "$src" && COPYFILE_DISABLE=1 tar --format=ustar -cf - "$folder") |
        on_device "mkdir -p '$dest' && tar -xf - -C '$dest' && sync"
    say "HI5 v$latest is installed. Launch it from $where."
}

# everything runs from here, so bash has read the whole script before `curl | bash` starts it
main "$@"
