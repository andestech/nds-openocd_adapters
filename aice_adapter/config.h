#ifndef __CONFIG_H_
#define __CONFIG_H_ 

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <unistd.h>
#include "aice_log.h"

#define LOG_ERROR   aice_log_error
#define LOG_DEBUG   aice_log_detail 
#define LOG_INFO    aice_log_detail
#define LOG_WARNING aice_log_error

#define ERROR_OK (0)
#define ERROR_FAIL (-4)

#include <errno.h>

#ifndef ETIMEDOUT
#define ETIMEDOUT WSAETIMEDOUT
#endif



#endif 
