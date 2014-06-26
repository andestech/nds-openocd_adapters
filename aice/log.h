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

#define PRIx32 "x"
#define PRId32 "d"
#define SCNx32 "x"
#define PRIi32 "i"
#define PRIu32 "u"
#define PRId8 PRId32
#define SCNx64 "llx"
#define PRIx64 "llx"

#define LOG_ERROR(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define DEBUG_JTAG_IO(fmt, ...) log_add (LOG_DEBUG, fmt"\n", ##__VA_ARGS__)

#endif