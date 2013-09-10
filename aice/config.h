#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>
#include <stdbool.h>

#ifndef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#endif

#if 0
#define false 0
#define true 1

typedef unsigned long int       uint64_t;
typedef unsigned int uint32_t;
typedef unsigned short uint16_t;
typedef unsigned char uint8_t;
typedef unsigned char bool;
#endif

#endif
