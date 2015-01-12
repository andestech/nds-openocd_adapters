#ifndef __LOG_H__
#define __LOG_H__

enum log_level_t {
	AICE_LOG_DISABLE = 0,
	AICE_LOG_VERBOSE,
	AICE_LOG_INFO,
	AICE_LOG_DEBUG,
};

void aice_log_init (uint32_t a_buf_size, uint32_t a_debug_level, bool a_unlimited);
void aice_log_finalize (void);
void aice_log_add (uint32_t a_level, const char *a_format, ...);
/*
#define PRIx32 "x"
#define PRId32 "d"
#define SCNx32 "x"
#define PRIi32 "i"
#define PRIu32 "u"
#define PRId8 PRId32
#define SCNx64 "llx"
#define PRIx64 "llx"

#define LOG_ERROR(fmt, ...) aice_log_add (AICE_LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) aice_log_add (AICE_LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) aice_log_add (AICE_LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define LOG_WARNING(fmt, ...) aice_log_add (AICE_LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
#define DEBUG_JTAG_IO(fmt, ...) aice_log_add (AICE_LOG_DEBUG, fmt"\n", ##__VA_ARGS__)
*/
#endif
