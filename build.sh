#!/bin/bash
set -e

FLAGS="`./gcc-tune-flags`"
FLAGS=""
FLAGS="$FLAGS -DDEBUG_WAKEUP"


CFLAGS=$FLAGS CXXFLAGS=$FLAGS LINKFLAGS=$LDFLAGS ./waf configure --alsa --portaudio=no --prefix=/usr

sleep 2

pidof jackd && killall jackd

./waf build -j6

echo "JACK2 built. Now run:"
echo "./waf install"
