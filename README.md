jack2 fork with changes:
- fixed ALSA asound linking on Raspberry Pi arch linux (TODO: need to investigage this and submit to jack2)


- Added support for Raspberry Pi 1 cyclce counter (needs to be enabled with special kernel module, otherwise jack will crash!)
Must be enabled with macro definition ARM11_CYCLE_COUNTER

There is also support for ARM 7/7A cycle counters (RPI2) which needs to be tested

Note: compiler will throw an error if neither ARM11_CYCLE_COUNTER nor (__ARM_ARCH_7A__ || __ARM_ARCH_7__) is defined!

For the cycle counter it reads /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_(max|min)_freq to convert cycles to time.
The min and max frequencies need to be equal, otherwise the counter is useless, so energy profile must be set to performance (disabled energy saving)


- Added support for the Raspberry PI 1 1Mhz HPET counter
source: http://mindplusplus.wordpress.com/2013/05/21/accessing-the-raspberry-pis-1mhz-timer/

jackd command line args:
"-cc" : use cycle counter
"-ch" : use HPET timer



Build with build.sh, which check if compiling on arm7 or higher. It will then use NEON intrinsics