#!/bin/sh
# Rebuild on the Haiku box, start the game, drive it with a few keys and
# collect the log plus screenshots. Needs SSH access to the box.
#
# Usage: tools/run-remote.sh [seconds-before-first-shot]
#   HAIKU_HOST=user@renku HAIKU_DIR=/boot/home/princeofpersia
SECS=${1:-10}
HOST=${HAIKU_HOST:-user@renku}
DIR=${HAIKU_DIR:-/boot/home/princeofpersia}
cd "$(dirname "$0")/.."
OUT=build/remote
mkdir -p "$OUT"
COPYFILE_DISABLE=1 tar czf - --no-xattrs Makefile SDLPoP.ini src resources data tools | \
ssh -o BatchMode=yes -o ConnectTimeout=20 "$HOST" "
	mkdir -p $DIR && cd $DIR && tar xzf - &&
	setarch x86 make 2>&1 | grep -E 'error' | head -20;
	setarch x86 g++ -O1 -o build/sendkey tools/sendkey.cpp -lbe 2>&1 | grep error;
	kill \$(ps | grep '[P]rinceOfPersia' | awk '{print \$(NF-3)}') 2>/dev/null; sleep 1;
	rm -f /tmp/pop.log;
	(POP_AUDIO_DEBUG=1 ./build/PrinceOfPersia > /tmp/pop.log 2>&1 &);
	sleep $SECS; screenshot -s /tmp/pop-title.png >/dev/null 2>&1;
	./build/sendkey key:47 sleep:3000; screenshot -s /tmp/pop-level1.png >/dev/null 2>&1;
	./build/sendkey mod:4b:1 key:61 mod:4b:0 sleep:1500; screenshot -s /tmp/pop-step.png >/dev/null 2>&1;
	echo '--- log:'; head -40 /tmp/pop.log;
	echo '--- running:'; ps | grep -c '[P]rinceOfPersia'
" 2>&1 | grep -v -E 'WARNING|store now|openssh.com'
for f in title level1 step; do
	scp -o BatchMode=yes -q "$HOST:/tmp/pop-$f.png" "$OUT/$f.png" 2>/dev/null && echo "screenshot: $OUT/$f.png"
done
