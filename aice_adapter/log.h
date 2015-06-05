#include <stdint.h>

#ifndef __LOG_H__
#define __LOG_H__

#define MINIMUM_DEBUG_LOG_SIZE 0x100000
#define WORKING_BUF_SIZE 4096


// Based on OpenOCD logger
enum log_level_t {
	AICE_LOG_SILENT  = -3,
	AICE_LOG_OUTPUT  = -2,
	AICE_LOG_USER    = -1,
	AICE_LOG_ERROR   = 0,
	AICE_LOG_WARNING = 1,
	AICE_LOG_INFO    = 2,
	AICE_LOG_DEBUG   = 3,
	AICE_LOG_DETAIL  = 4,
};

void aice_log_init (uint32_t a_buf_size, uint32_t a_debug_level);
void aice_log_finalize (void);
void aice_log_add (uint32_t a_level, const char *a_format, ...);

#define aice_log_error(fmt, ...) aice_log_add (AICE_LOG_ERROR, fmt"\n", ##__VA_ARGS__)
#define aice_log_debug(fmt, ...) aice_log_add (AICE_LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define aice_log_info(fmt, ...) aice_log_add (AICE_LOG_INFO, fmt"\n", ##__VA_ARGS__)
#define aice_log_detail(fmt, ...) aice_log_add (AICE_LOG_DETAIL, fmt"\n", ##__VA_ARGS__)

void alive_sleep(uint64_t ms);

#define LOG_ERROR   aice_log_error
#define LOG_DEBUG   aice_log_detail 
#define LOG_INFO    aice_log_detail
#define LOG_WARNING aice_log_error

#define keep_alive()   

#endif
