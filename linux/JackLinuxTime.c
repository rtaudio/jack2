/*
Copyright (C) 2001-2003 Paul Davis
Copyright (C) 2005 Jussi Laako
Copyright (C) 2004-2008 Grame

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

*/

#include "JackConstants.h"
#include "JackTime.h"
#include "JackTypes.h"
#include "JackError.h"
#include "cycles.h"

#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>

static jack_time_t __jack_cpu_mhz = 0;
jack_time_t (*_jack_get_microseconds)(void) = 0;

#if defined(__gnu_linux__) && (defined(__i386__) || defined(__x86_64__))
#define HPET_SUPPORT
#define HPET_MMAP_SIZE		1024
#define HPET_CAPS			0x000
#define HPET_PERIOD			0x004
#define HPET_COUNTER		0x0f0
#define HPET_CAPS_COUNTER_64BIT	(1 << 13)
#if defined(__x86_64__)
typedef uint64_t hpet_counter_t;
#else
typedef uint32_t hpet_counter_t;
#endif
static int hpet_fd;
static unsigned char *hpet_ptr;
static uint32_t hpet_period; /* period length in femto secs */
	static uint64_t hpet_offset = 0;
	static uint64_t hpet_wrap;
	static hpet_counter_t hpet_previous = 0;
#elif defined(__arm__) && defined(ARM_RPI2_HPET) /* defined(__gnu_linux__) && (__i386__ || __x86_64__) */
	#define HPET_SUPPORT
	#define BCM2709_PERI_BASE        0x3F000000 // rpi2
	#define ARM_PERI_BASE BCM2709_PERI_BASE
	#define ARM_HPET_ST_BASE                  (ARM_PERI_BASE + 0x3000)
	#define ARM_HPET_TIMER_OFFSET (4)
	#define ARM_HPET_TIMER_RATE 1000000
	#warning "Note: choosing RPI2 hpet timer offset (BCM2709)"
#endif



#if defined(__arm__) && defined(HPET_SUPPORT)
static long long int *arm_hpet_ptr;

static int jack_hpet_init ()
{
	jack_log("Init ARM HPET at %#010x", ARM_HPET_ST_BASE);
	 
	int fd;
	void *st_base;
	
    if (-1 == (fd = open("/dev/mem", O_RDONLY))) {
        printf ("Cannot access /dev/mem (%s)\n", strerror (errno));
        return -1;
    }
	
    if (MAP_FAILED == (st_base = mmap(NULL, 4096,
                        PROT_READ, MAP_SHARED, fd, ARM_HPET_ST_BASE))) {
        printf ("mmap() failed.\n");
        return -1;
    }
	
	arm_hpet_ptr = (long long int *)((char *)st_base + ARM_HPET_TIMER_OFFSET);
	
	return 0;
}

jack_time_t jack_get_microseconds_from_hpet (void)
{
	return *arm_hpet_ptr; // 1mhz counter => 1µs cycle
}
#elif defined(HPET_SUPPORT)
static int jack_hpet_init ()
{
	uint32_t hpet_caps;

	hpet_fd = open("/dev/hpet", O_RDONLY);
	if (hpet_fd < 0) {
		jack_error ("This system has no accessible HPET device (%s)", strerror (errno));
		return -1;
	}

	hpet_ptr = (unsigned char *) mmap(NULL, HPET_MMAP_SIZE,
					  PROT_READ, MAP_SHARED, hpet_fd, 0);
	if (hpet_ptr == MAP_FAILED) {
		jack_error ("This system has no mappable HPET device (%s)", strerror (errno));
		close (hpet_fd);
		return -1;
	}

	/* this assumes period to be constant. if needed,
	   it can be moved to the clock access function
	*/
	hpet_period = *((uint32_t *) (hpet_ptr + HPET_PERIOD));
	hpet_caps = *((uint32_t *) (hpet_ptr + HPET_CAPS));
	hpet_wrap = ((hpet_caps & HPET_CAPS_COUNTER_64BIT) &&
		(sizeof(hpet_counter_t) == sizeof(uint64_t))) ?
		0 : ((uint64_t) 1 << 32);

	return 0;
}

static jack_time_t jack_get_microseconds_from_hpet (void)
{
	hpet_counter_t hpet_counter;
	long double hpet_time;

	hpet_counter = *((hpet_counter_t *) (hpet_ptr + HPET_COUNTER));
	if (hpet_counter < hpet_previous)
		hpet_offset += hpet_wrap;
	hpet_previous = hpet_counter;
	hpet_time = (long double) (hpet_offset + hpet_counter) *
		(long double) hpet_period * (long double) 1e-9;
	return ((jack_time_t) (hpet_time + 0.5));
}

#else

static int jack_hpet_init ()
{
	jack_error ("This version of JACK or this computer does not have HPET support.\n"
		    "Please choose a different clock source.");
	return -1;
}

static jack_time_t jack_get_microseconds_from_hpet (void)
{
	/* never called */
	return 0;
}

#endif /* HPET_SUPPORT */

static jack_time_t jack_get_microseconds_from_cycles (void) {
	return get_cycles() / __jack_cpu_mhz;
}


 
static jack_time_t jack_read_file_u64(const char *filename)
{
	jack_time_t mhz;
	
	char buf[60];
	int ret;
	
	FILE *f = fopen(filename, "r");
	
	if(!f)
		return 0;	
	
	ret = fgets(buf, sizeof(buf), f);
	fclose(f);
	
	if(!ret)
		return 0;
		
	if(sscanf(buf, "%" SCNu64, &mhz) == 1)
		return mhz;
		
	return 0;
}

/*
 * This is another kludge.  It looks CPU-dependent, but actually it
 * reflects the lack of standards for the Linux kernel formatting of
 * /proc/cpuinfo.
 */
static jack_time_t jack_get_mhz (void)
{
/*
	int ret = 0;
	FILE *f = fopen("/proc/cpuinfo", "r");
	if (f)
	{
		if(fgets(buf, sizeof(buf), f))
		{
#if defined(__powerpc__)
		ret = sscanf(buf, "clock\t: %" SCNu64 "MHz", &mhz);
#elif defined( __i386__ ) || defined (__hppa__)  || defined (__ia64__) || \
      defined(__x86_64__)
		ret = sscanf(buf, "cpu MHz         : %" SCNu64, &mhz);
#elif defined( __sparc__ )
		ret = sscanf(buf, "Cpu0Bogo        : %" SCNu64, &mhz);
#elif defined( __mc68000__ )
		ret = sscanf(buf, "Clocking:       %" SCNu64, &mhz);
#elif defined( __s390__  )
		ret = sscanf(buf, "bogomips per cpu: %" SCNu64, &mhz);
#elif defined( __sh__  )
		ret = sscanf(buf, "bogomips        : %" SCNu64, &mhz);
#else // MIPS, ARM, alpha
		ret = 0;
#endif			
		}
		fclose(f);
	}
	
	if (ret == 1)
		return (jack_time_t)mhz;
	*/

		
	jack_time_t  khz_max = jack_read_file_u64("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
	jack_time_t  khz_min = jack_read_file_u64("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_min_freq");
	
	if(khz_max == 0) {
		jack_error("FATAL: Cant't read /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq!\n");
		exit(1);		
	}
	
	if(khz_max != khz_min)
	{
		jack_error("FATAL: Cant't use cycle counter, cpu frequencies can vary!\n");
		exit(1);	
	}
	
	jack_time_t mhz = khz_max / 1000;
	
	jack_log("CPU MHz : %d", (int)mhz);
	return (jack_time_t)mhz;
}


#ifdef ARM_HPET

// ARM HPET from http://mindplusplus.wordpress.com/2013/05/21/accessing-the-raspberry-pis-1mhz-timer/

#define ST_BASE (0x20003000)
#define TIMER_OFFSET (4)
#define TIMER_RATE 1000000

static long long int *arm_hpet_ptr;

static int jack_arm_hpet_init ()
{
	int fd;
	void *st_base;
	
    // get access to system core memory
    if (-1 == (fd = open("/dev/mem", O_RDONLY))) {
        jack_error ("Cannot access /dev/mem (%s)", strerror (errno));
        return -1;
    }
	
    // map a specific page into process's address space
    if (MAP_FAILED == (st_base = mmap(NULL, 4096,
                        PROT_READ, MAP_SHARED, fd, ST_BASE))) {
        jack_error ("mmap() failed.\n");
        return -1;
    }
	
	arm_hpet_ptr = (long long int *)((char *)st_base + TIMER_OFFSET);
	

	return 0;
}

static jack_time_t jack_get_microseconds_from_arm_hpet (void)
{
	return *arm_hpet_ptr; // 1mhz counter => 1µs cycle
}
#endif

#define HAVE_CLOCK_GETTIME 1

#ifndef HAVE_CLOCK_GETTIME

static jack_time_t jack_get_microseconds_from_system (void)
{
	jack_time_t jackTime;
	struct timeval tv;

	gettimeofday (&tv, NULL);
	jackTime = (jack_time_t) tv.tv_sec * 1000000 + (jack_time_t) tv.tv_usec;
	return jackTime;
}

#else

static jack_time_t jack_get_microseconds_from_system (void)
{
	jack_time_t jackTime;
	struct timespec time;

	clock_gettime(CLOCK_MONOTONIC, &time);
	jackTime = (jack_time_t) time.tv_sec * 1e6 +
		(jack_time_t) time.tv_nsec / 1e3;
	return jackTime;
}

#endif /* HAVE_CLOCK_GETTIME */


SERVER_EXPORT void JackSleep(long usec)
{
	usleep(usec);
}

SERVER_EXPORT void InitTime()
{
	// initialization done in SetClockSource if necessary
}

SERVER_EXPORT void EndTime()
{}

void SetClockSource(jack_timer_type_t source)
{
    jack_log("Clock source : %s", ClockSourceName(source));

	switch (source)
	{
        case JACK_TIMER_CYCLE_COUNTER:
			jack_log("Clock source : %s", "(arm) cycle counter");
			__jack_cpu_mhz = jack_get_mhz ();
            _jack_get_microseconds = jack_get_microseconds_from_cycles;
            break;

        case JACK_TIMER_HPET:
            if (jack_hpet_init () == 0) {
                _jack_get_microseconds = jack_get_microseconds_from_hpet;
            } else {
                _jack_get_microseconds = jack_get_microseconds_from_system;
            }
            break;

        case JACK_TIMER_SYSTEM_CLOCK:
            default:
            _jack_get_microseconds = jack_get_microseconds_from_system;
            break;
	}
}

const char* ClockSourceName(jack_timer_type_t source)
{
	switch (source) {
        case JACK_TIMER_CYCLE_COUNTER:
            return "cycle counter";
        case JACK_TIMER_HPET:
            return "hpet";
        case JACK_TIMER_SYSTEM_CLOCK:
        #ifdef HAVE_CLOCK_GETTIME
            return "system clock via clock_gettime";
        #else
            return "system clock via gettimeofday";
        #endif
	}

	/* what is wrong with gcc ? */
	return "unknown";
}

SERVER_EXPORT jack_time_t GetMicroSeconds()
{
	return _jack_get_microseconds();
}

SERVER_EXPORT jack_time_t jack_get_microseconds()
{
	return _jack_get_microseconds();
}

