#!/bin/bash
set -e

#./waf clean

# opkg install libsamplerate0

#check for neon
if ! [[ -z `lscpu | grep Architecture | grep -oEi ':.+' | grep -oEi 'armv[7-9]'` ]]; then HAVE_NEON=true; else HAVE_NEON=false; fi

if $HAVE_NEON; then
	echo "NEON found"
	HARD_OPT="-O3 -fomit-frame-pointer -ffast-math -funroll-all-loops -ftree-vectorize -mfloat-abi=hard -mfpu=neon -lasound" # -DARM_HPET" # `xeno-config --skin=posix --cflags`"
else
	echo "NEON NOT found"
	HARD_OPT="-O3 -fomit-frame-pointer -ffast-math -funroll-all-loops -ftree-vectorize -mfloat-abi=hard -mfpu=vfp -DARM_HPET -DARM11_CYCLE_COUNTER" # `xeno-config --skin=posix --cflags`"
fi

CXXFLAGS=$HARD_OPT CFLAGS=$HARD_OPT ./waf configure --alsa --portaudio=no --prefix=/usr

#./waf build -j6

echo "JACK2 configured. Now run:"
echo "./waf build -j6"
echo "sudo ./waf install"
