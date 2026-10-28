#!/bin/sh
# HI5 launcher for TrimUI OS (TrimUI Brick, Brick Hammer and Brick Pro).
# All four cores stay online, at full speed, while it plays.
cd "$(dirname "$0")"

CPU=/sys/devices/system/cpu
gov_was=$(cat $CPU/cpu0/cpufreq/scaling_governor 2>/dev/null)
for c in 0 1 2 3; do
    [ -f $CPU/cpu$c/online ] && echo 1 > $CPU/cpu$c/online 2>/dev/null
done
echo performance > $CPU/cpu0/cpufreq/scaling_governor 2>/dev/null

export LD_LIBRARY_PATH="/usr/trimui/lib:/usr/lib:$LD_LIBRARY_PATH"

# Optional settings, before ./hi5 (for example HI5_BUFFER=1024 ./hi5):
#   HI5_BUFFER=1024     bigger audio buffers if you hear crackle (512 by default)
#   HI5_RATE=44100      a lower sample rate: a little less work
#   HI5_UPDATE=0        don't check GitHub for a new version
#   HI5_AUDIO=internal  always the speaker and headphones (default: a USB audio device when one is plugged in)
#   HI5_AUDIO=<name>    the output device whose name contains <name> (see log.txt)
#   HI5_MIDI=0          no USB MIDI
#   HI5_MIDI_CLOCK=0    send no MIDI clock;  HI5_MIDI_FOLLOW=0  ignore MIDI clock coming in
#   HI5_SYNC_LATENCY=n  Wi-Fi sync: the frames the sound driver holds (the buffer size by default)
#   HI5_ROTATE=180      turn the picture, for a firmware whose screen is turned
#   HI5_JOYMAP=A=1,B=0  another firmware's button numbers
# exit status 75: the app has just updated itself, so start the new version
while :; do
    ./hi5 > log.txt 2>&1
    [ $? -eq 75 ] || break
done

[ -n "$gov_was" ] && echo "$gov_was" > $CPU/cpu0/cpufreq/scaling_governor 2>/dev/null
