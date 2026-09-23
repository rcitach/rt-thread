#ifndef WIFI_CONFIG_TIME_H
#define WIFI_CONFIG_TIME_H

#include <stdint.h>

/* RT-Thread's libc sys/types.h includes time.h while defining its own time
 * types. Declare the Newlib-compatible type before entering that include
 * cycle, then continue with the toolchain header. */
#ifndef __time_t_defined
typedef int64_t time_t;
#define __time_t_defined
#endif

#ifndef _TIME_T_DECLARED
#define _TIME_T_DECLARED
#endif

#ifndef _SUSECONDS_T_DECLARED
typedef long suseconds_t;
#define _SUSECONDS_T_DECLARED
#endif

#include_next <time.h>

#endif
