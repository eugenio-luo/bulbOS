#ifndef _TIME_H
#define _TIME_H 1

#include <sys/cdefs.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {}
#endif

typedef int32_t time_t;

time_t time(time_t *tloc);

#ifdef __cplusplus
}
#endif

#endif
