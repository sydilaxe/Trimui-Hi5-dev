#!/bin/sh
# The self-update end to end, without the internet (make update-test; CI runs it).
#
# tools/fake_github.py serves a v9.9.9 release the way GitHub does: the API's JSON, a
# ~1000-character signed redirect, then the MustardOS package (.muxapp, a zip).
# build/hi5_update installs it over an older app folder, as the app does when you press
# A. First, the same release with a damaged zip must fail and leave the folder as it was.
set -eu

T=build/update-test
PORT=${PORT:-8767}
rm -rf $T
mkdir -p "$T/rel/HI5/samples/KIT" $T/bad $T/app/samples

# the release: launch.sh, mux_launch.sh and friends, a binary, two samples (one in a
# folder), and a hi5.sav that must never reach the user's folder
R="$T/rel/HI5"
cp dist/HI5/launch.sh dist/HI5/cacert.pem dist/mustardos/mux_launch.sh "$R/"
cp build/hi5_update "$R/hi5"
echo 9.9.9 > "$R/VERSION"
head -c 300000 /dev/urandom > "$R/samples/LOOP.wav"
head -c 20000 /dev/urandom > "$R/samples/KIT/001.wav"
echo theirs > "$R/hi5.sav"
Z=HI5-mustardos-brick.muxapp
(cd $T/rel && zip -qr $Z "HI5")

# the device's folder: an older version, the user's song and sample, and one sample
# that's already the same as the release's (dated 2001, so we can tell it's left alone)
A=$T/app
cp dist/HI5/cacert.pem $A/
echo old > $A/mux_launch.sh
echo old > $A/hi5
echo old > $A/launch.sh
echo 0.1.0 > $A/VERSION
echo mine > $A/hi5.sav
head -c 1000 /dev/urandom > $A/samples/MINE.wav
cp "$R/samples/LOOP.wav" $A/samples/
touch -t 200101010000 $A/samples/LOOP.wav

# the same release with its zipped binary damaged (the CRC check must catch it)
cp $T/rel/$Z $T/bad/
off=$(python3 -c '
import sys, zipfile
z = zipfile.ZipFile(sys.argv[1]); i = z.getinfo("HI5/hi5")
print(i.header_offset + 30 + len(i.filename) + len(i.extra) + i.compress_size // 2)' $T/bad/$Z)
printf 'XXXXXXXX' | dd of=$T/bad/$Z bs=1 seek="$off" conv=notrunc 2>/dev/null

python3 tools/fake_github.py $T/rel --port $PORT --tag v9.9.9 --asset $Z &
good=$!
python3 tools/fake_github.py $T/bad --port $((PORT + 1)) --tag v9.9.9 --asset $Z &
bad=$!
trap 'kill $good $bad 2>/dev/null || true' EXIT
for i in $(seq 1 50); do
    curl -s -o /dev/null http://127.0.0.1:$PORT/latest.json && curl -s -o /dev/null http://127.0.0.1:$((PORT + 1))/latest.json && break
    sleep 0.1
done

fail() { echo "update test: $*" >&2; exit 1; }

cp -r $A $T/before
if HI5_UPDATE_API=http://127.0.0.1:$((PORT + 1))/latest.json build/hi5_update $A; then
    fail "a damaged zip was installed"
fi
diff -r $T/before $A >/dev/null || fail "a failed update changed the app folder"
echo "update test: a damaged zip is refused and changes nothing"

HI5_UPDATE_API=http://127.0.0.1:$PORT/latest.json build/hi5_update $A
cmp -s $A/hi5 "$R/hi5" || fail "the binary wasn't replaced"
test -x $A/hi5 || fail "the binary isn't executable"
cmp -s $A/launch.sh "$R/launch.sh" || fail "launch.sh wasn't replaced"
cmp -s $A/mux_launch.sh "$R/mux_launch.sh" || fail "mux_launch.sh wasn't replaced"
test -x $A/mux_launch.sh || fail "mux_launch.sh isn't executable"
test "$(cat $A/VERSION)" = 9.9.9 || fail "VERSION isn't 9.9.9"
cmp -s $A/samples/KIT/001.wav "$R/samples/KIT/001.wav" || fail "a sample folder wasn't made"
test "$(cat $A/hi5.sav)" = mine || fail "the user's song was overwritten"
test -f $A/samples/MINE.wav || fail "the user's sample is gone"
test "$(date -r $A/samples/LOOP.wav +%Y)" = 2001 || fail "an unchanged sample was rewritten"
test -z "$(find $A -name '*.new')" || fail ".new files were left behind"
echo "update test: installed v9.9.9 (the .muxapp) over v0.1.0, kept the song and the user's sample, skipped the unchanged sample"
