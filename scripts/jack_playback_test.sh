pidof jackd || ( jackd --realtime -P40 --sync -dalsa -dhw:$1 -p256 -o2 -i2 -n3 -r44100 -s & && sleep 2 )
jack_metro 2
jack_connect system:capture_1 system:playback_1 && jack_connect system:capture_2 system:playback_2
