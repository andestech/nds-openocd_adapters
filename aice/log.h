#ifndef __LOG_H__
#define __LOG_H__

enum log_level_t {
	LOG_DISABLE = 0,
	LOG_VERBOSE,
	LOG_INFO,
	LOG_DEBUG,
};

void log_init (uint32_t a_buf_size, uint32_t a_debug_level, bool a_unlimited);
void log_finalize (void);
void log_add (uint32_t a_level, const char *a_format, ...);

#endif
