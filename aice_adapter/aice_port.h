#ifndef _AICE_PORT_H_
#define _AICE_PORT_H_

#define ERROR_OK 		(0)
#define ERROR_FAIL 		(-1)
#define ERROR_DISCONNECT	(-2)
#define ERROR_AICE_TIMEOUT	(-3)


#define MAX_ID_CODE 	(16)

enum aice_target_state_s {
	AICE_DISCONNECT = 0,
	AICE_TARGET_DETACH,
	AICE_TARGET_UNKNOWN,
	AICE_TARGET_RUNNING,
	AICE_TARGET_HALTED,
	AICE_TARGET_RESET,
	AICE_TARGET_DEBUG_RUNNING,
};

enum aice_srst_type_s {
	AICE_SRST = 0x1,
	AICE_RESET_HOLD = 0x8,
};

enum aice_api_s {
	AICE_OPEN = 0x0,
	AICE_CLOSE,
	AICE_RESET,
	AICE_ASSERT_SRST,
	AICE_RUN,
	AICE_HALT,
	AICE_STEP,
	AICE_READ_REG,
	AICE_WRITE_REG,
	AICE_READ_REG_64,
	AICE_WRITE_REG_64,
	AICE_READ_MEM_UNIT,
	AICE_WRITE_MEM_UNIT,
	AICE_READ_MEM_BULK,
	AICE_WRITE_MEM_BULK,
	AICE_READ_DEBUG_REG,
	AICE_WRITE_DEBUG_REG,
	AICE_IDCODE,
	AICE_STATE,
	AICE_SET_JTAG_CLOCK,
	AICE_SELECT_TARGET,
	AICE_MEMORY_ACCESS,
	AICE_MEMORY_MODE,
	AICE_READ_TLB,
	AICE_CACHE_CTL,
};

enum aice_error_s {
	AICE_OK,
	AICE_ACK,
	AICE_ERROR,
};

enum aice_memory_access {
	AICE_MEMORY_ACC_BUS = 0,
	AICE_MEMORY_ACC_CPU,
};

enum aice_memory_mode {
	AICE_MEMORY_MODE_AUTO = 0,
	AICE_MEMORY_MODE_MEM = 1,
	AICE_MEMORY_MODE_ILM = 2,
	AICE_MEMORY_MODE_DLM = 3,
};

enum aice_cache_ctl_type {
	AICE_CACHE_CTL_L1D_INVALALL = 0,
	AICE_CACHE_CTL_L1D_VA_INVAL,
	AICE_CACHE_CTL_L1D_WBALL,
	AICE_CACHE_CTL_L1D_VA_WB,
	AICE_CACHE_CTL_L1I_INVALALL,
	AICE_CACHE_CTL_L1I_VA_INVAL,
};

const static const char *AICE_MEMORY_ACCESS_NAME[] = {
	"BUS",
	"CPU",
};

const static const char *AICE_MEMORY_MODE_NAME[] = {
	"AUTO",
	"MEM",
	"ILM",
	"DLM",
};

static inline void set_u32(void *_buffer, uint32_t value)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	buffer[3] = (value >> 24) & 0xff;
	buffer[2] = (value >> 16) & 0xff;
	buffer[1] = (value >> 8) & 0xff;
	buffer[0] = (value >> 0) & 0xff;
}

static inline uint32_t get_u32(const void *_buffer)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	return (((uint32_t)buffer[3]) << 24) |
		(((uint32_t)buffer[2]) << 16) |
		(((uint32_t)buffer[1]) << 8) |
		(((uint32_t)buffer[0]) << 0);
}

static inline void set_u16(void *_buffer, uint16_t value)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	buffer[1] = (value >> 8) & 0xff;
	buffer[0] = (value >> 0) & 0xff;
}

static inline uint16_t get_u16(const void *_buffer)
{
	uint8_t *buffer = (uint8_t *)_buffer;

	return (((uint16_t)buffer[1]) << 8) |
		(((uint16_t)buffer[0]) << 0);
}

#endif
