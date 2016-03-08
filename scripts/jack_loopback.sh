pidof jackd || ./jack_start.sh
jack_connect system:capture_1 system:playback_1 && jack_connect system:capture_2 system:playback_2
