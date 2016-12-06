#!/bin/bash
set -e

FLAGS="`./gcc-tune-flags`"
#FLAGS="`./gcc-tune-flags`"
FLAGS="-Ofast"
#FLAGS="$FLAGS -DDEBUG_WAKEUP"

FLAGS="$FLAGS -DDEBUG_WAKEUP"


CFLAGS=$FLAGS CXXFLAGS=$FLAGS LINKFLAGS=$LDFLAGS ./waf configure --alsa --portaudio=no --prefix=/usr

sleep 2

killall jackd
pidof jackd && killall jackd

./waf build -j6

echo "JACK2 built. Now run:"
echo "./waf install"

echo ""
echo "Make sure to remove the following packages: gjacktransport qjackctl qjackrcd libjack-dev libjack-jackd2-0 libjack0 libjack-jackd2-dev"


# jack default cmd line: 
#
#jackd -v -ch --realtime -P40 --sync -dalsa -dhw:1,0 -p48 -o2 -i2 -n2 -r48000
#jackd --realtime -P40 --sync -dalsa -dhw:1,0 -p48 -o2 -i2 -n2 -r48000
#jack_connect system:capture_1 system:playback_1 && jack_connect system:capture_2 system:playback_2
#jackd --realtime -P40 --sync -dalsa -dhw:1,0 -p48 -o2 -i2 -n2 -r48000
# 
