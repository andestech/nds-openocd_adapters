#ifndef __CONFIG_H_
#define __CONFIG_H_ 

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include "log.h"

#define ERROR_OK (0)
#define ERROR_FAIL (-4)

#include <errno.h>

#ifndef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#endif



#endif 
