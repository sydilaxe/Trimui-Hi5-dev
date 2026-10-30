#!/bin/sh
# HI5 launcher for the TrimUI Brick, Brick Hammer and Brick Pro: TrimUI OS runs it
# directly, MustardOS through mux_launch.sh.
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
#   HI5_LEDS=0          Brick Pro: leave the lights alone;  HI5_LED_BAR=0  keep the top bar dark
#   HI5_LED_BRIGHTNESS=30  Brick Pro: the lights' brightness, 1..60 (60 by default)
#   HI5_JOYMAP=A=1,B=0  another firmware's button numbers
#   HI5_LOG=2           the performance log in log.txt every second, and every button press (1: every
#                       10 s, the default; 0: off). HI5_SYSINFO=0: no sysinfo.txt (see sysinfo.sh)
[ "${HI5_SYSINFO:-1}" != 0 ] && [ "${HI5_LOG:-1}" != 0 ] && ./sysinfo.sh start hi5
# exit status 75: the app has just updated itself, so start the new version
while :; do
    ./hi5 > log.txt 2>&1
    [ $? -eq 75 ] || break
done

./sysinfo.sh stop hi5
[ -n "$gov_was" ] && echo "$gov_was" > $CPU/cpu0/cpufreq/scaling_governor 2>/dev/null
