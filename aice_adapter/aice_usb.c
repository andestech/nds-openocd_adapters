#include "config.h"

#include <string.h>
#include <errno.h>
#include "aice_usb.h"
#include "log.h"

/* Global USB buffers */
static uint8_t usb_in_buffer[AICE_IN_BUFFER_SIZE];
static uint8_t usb_out_buffer[AICE_OUT_BUFFER_SIZE];
static struct aice_usb_handler_s aice_handler;

/***************************************************************************/
/* AICE commands' pack/unpack functions */
static void aice_pack_htda(uint8_t cmd_code, uint8_t extra_word_length, uint32_t address)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = extra_word_length;
	usb_out_buffer[2] = (uint8_t)(address & 0xFF);
}

static void aice_pack_htdb(uint8_t cmd_code, uint8_t extra_word_length, uint32_t address)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = extra_word_length;
	usb_out_buffer[2] = (uint8_t)((address >> 24) & 0xFF);
	usb_out_buffer[3] = (uint8_t)((address >> 16) & 0xFF);
	usb_out_buffer[4] = (uint8_t)((address >> 8) & 0xFF);
	usb_out_buffer[5] = (uint8_t)(address & 0xFF);
}

static void aice_pack_htdc(uint8_t cmd_code, uint8_t extra_word_length, uint32_t address, uint32_t word)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = extra_word_length;
	usb_out_buffer[2] = (uint8_t)(address & 0xFF);
	usb_out_buffer[3] = (uint8_t)((word >> 24) & 0xFF);
	usb_out_buffer[4] = (uint8_t)((word >> 16) & 0xFF);
	usb_out_buffer[5] = (uint8_t)((word >> 8) & 0xFF);
	usb_out_buffer[6] = (uint8_t)(word & 0xFF);
}

static void aice_pack_htdd(uint8_t cmd_code, uint8_t extra_word_length, uint32_t address, uint32_t word)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = extra_word_length;
	usb_out_buffer[2] = (uint8_t)((address >> 24) & 0xFF);
	usb_out_buffer[3] = (uint8_t)((address >> 16) & 0xFF);
	usb_out_buffer[4] = (uint8_t)((address >> 8) & 0xFF);
	usb_out_buffer[5] = (uint8_t)(address & 0xFF);
	usb_out_buffer[6] = (uint8_t)((word >> 24) & 0xFF);
	usb_out_buffer[7] = (uint8_t)((word >> 16) & 0xFF);
	usb_out_buffer[8] = (uint8_t)((word >> 8) & 0xFF);
	usb_out_buffer[9] = (uint8_t)(word & 0xFF);
}

static void aice_pack_htdma(uint8_t cmd_code, uint8_t target_id, uint8_t extra_word_length, uint32_t address)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = target_id;
	usb_out_buffer[2] = extra_word_length;
	usb_out_buffer[3] = (uint8_t)(address & 0xFF);
}

static void aice_pack_htdmb(uint8_t cmd_code, uint8_t target_id, uint8_t extra_word_length, uint32_t address)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = target_id;
	usb_out_buffer[2] = extra_word_length;
	usb_out_buffer[3] = 0;
	usb_out_buffer[4] = (uint8_t)((address >> 24) & 0xFF);
	usb_out_buffer[5] = (uint8_t)((address >> 16) & 0xFF);
	usb_out_buffer[6] = (uint8_t)((address >> 8) & 0xFF);
	usb_out_buffer[7] = (uint8_t)(address & 0xFF);
}

static void aice_pack_htdmc(uint8_t cmd_code, uint8_t target_id, uint8_t extra_word_length, uint32_t address, uint32_t word)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = target_id;
	usb_out_buffer[2] = extra_word_length;
	usb_out_buffer[3] = (uint8_t)(address & 0xFF);
	usb_out_buffer[4] = (uint8_t)((word >> 24) & 0xFF);
	usb_out_buffer[5] = (uint8_t)((word >> 16) & 0xFF);
	usb_out_buffer[6] = (uint8_t)((word >> 8) & 0xFF);
	usb_out_buffer[7] = (uint8_t)(word & 0xFF);
}

static void aice_pack_htdmc_multiple_data(uint8_t cmd_code, uint8_t target_id, uint8_t extra_word_length, uint32_t address, uint32_t *word, uint8_t num_of_words)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = target_id;
	usb_out_buffer[2] = extra_word_length;
	usb_out_buffer[3] = (uint8_t)(address & 0xFF);

	uint8_t i;
	for (i = 0 ; i < num_of_words ; i++, word++)
	{
		usb_out_buffer[4 + i*4] = (uint8_t)((*word >> 24) & 0xFF);
		usb_out_buffer[5 + i*4] = (uint8_t)((*word >> 16) & 0xFF);
		usb_out_buffer[6 + i*4] = (uint8_t)((*word >> 8) & 0xFF);
		usb_out_buffer[7 + i*4] = (uint8_t)(*word & 0xFF);
	}
}

static void aice_pack_htdmd(uint8_t cmd_code, uint8_t target_id, uint8_t extra_word_length, uint32_t address, uint32_t word)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = target_id;
	usb_out_buffer[2] = extra_word_length;
	usb_out_buffer[3] = 0;
	usb_out_buffer[4] = (uint8_t)((address >> 24) & 0xFF);
	usb_out_buffer[5] = (uint8_t)((address >> 16) & 0xFF);
	usb_out_buffer[6] = (uint8_t)((address >> 8) & 0xFF);
	usb_out_buffer[7] = (uint8_t)(address & 0xFF);
	usb_out_buffer[8] = (uint8_t)((word >> 24) & 0xFF);
	usb_out_buffer[9] = (uint8_t)((word >> 16) & 0xFF);
	usb_out_buffer[10] = (uint8_t)((word >> 8) & 0xFF);
	usb_out_buffer[11] = (uint8_t)(word & 0xFF);
}

static void aice_pack_htdmd_multiple_data(uint8_t cmd_code, uint8_t target_id, uint8_t extra_word_length, uint32_t address, const uint32_t *word)
{
	usb_out_buffer[0] = cmd_code;
	usb_out_buffer[1] = target_id;
	usb_out_buffer[2] = extra_word_length;
	usb_out_buffer[3] = 0;
	usb_out_buffer[4] = (uint8_t)((address >> 24) & 0xFF);
	usb_out_buffer[5] = (uint8_t)((address >> 16) & 0xFF);
	usb_out_buffer[6] = (uint8_t)((address >> 8) & 0xFF);
	usb_out_buffer[7] = (uint8_t)(address & 0xFF);

	uint32_t i;
	uint32_t num_of_words = extra_word_length + 1;  // num_of_words may be over 0xFF, so use uint32_t
	for (i = 0 ; i < num_of_words ; i++, word++)
	{
		usb_out_buffer[8 + i*4] = (uint8_t)((*word >> 24) & 0xFF);
		usb_out_buffer[9 + i*4] = (uint8_t)((*word >> 16) & 0xFF);
		usb_out_buffer[10 + i*4] = (uint8_t)((*word >> 8) & 0xFF);
		usb_out_buffer[11 + i*4] = (uint8_t)(*word & 0xFF);
	}
}

static void aice_unpack_dtha(uint8_t *cmd_ack_code, uint8_t *extra_word_length, uint32_t *word)
{
	*cmd_ack_code = usb_in_buffer[0];
	*extra_word_length = usb_in_buffer[1];
	*word = (usb_in_buffer[2] << 24) |
		(usb_in_buffer[3] << 16) |
		(usb_in_buffer[4] << 8) |
		(usb_in_buffer[5]);
}

static void aice_unpack_dtha_multiple_data(uint8_t *cmd_ack_code, uint8_t *extra_word_length, uint32_t *word, uint8_t num_of_words)
{
	*cmd_ack_code = usb_in_buffer[0];
	*extra_word_length = usb_in_buffer[1];

	uint8_t i;
	for (i = 0 ; i < num_of_words ; i++, word++)
	{
		*word = (usb_in_buffer[2 + i*4] << 24) |
			(usb_in_buffer[3 + i*4] << 16) |
			(usb_in_buffer[4 + i*4] << 8) |
			(usb_in_buffer[5 + i*4]);
	}
}

static void aice_unpack_dthb(uint8_t *cmd_ack_code, uint8_t *extra_word_length)
{
	*cmd_ack_code = usb_in_buffer[0];
	*extra_word_length = usb_in_buffer[1];
}

static void aice_unpack_dthma(uint8_t *cmd_ack_code, uint8_t *target_id, uint8_t *extra_word_length, uint32_t *word)
{
	*cmd_ack_code = usb_in_buffer[0];
	*target_id = usb_in_buffer[1];
	*extra_word_length = usb_in_buffer[2];
	*word = (usb_in_buffer[4] << 24) |
		(usb_in_buffer[5] << 16) |
		(usb_in_buffer[6] << 8) |
		(usb_in_buffer[7]);
}

static void aice_unpack_dthma_multiple_data(uint8_t *cmd_ack_code, uint8_t *target_id, uint8_t *extra_word_length, uint32_t *word)
{
	*cmd_ack_code = usb_in_buffer[0];
	*target_id = usb_in_buffer[1];
	*extra_word_length = usb_in_buffer[2];
	*word = (usb_in_buffer[4] << 24) |
		(usb_in_buffer[5] << 16) |
		(usb_in_buffer[6] << 8) |
		(usb_in_buffer[7]);
	word++;

	uint8_t i;
	for (i = 0; i < *extra_word_length; i++)
	{
		*word = (usb_in_buffer[8 + i * 4] << 24) |
			(usb_in_buffer[9 + i * 4] << 16) |
			(usb_in_buffer[10 + i * 4] << 8) |
			(usb_in_buffer[11 + i * 4]);
		word++;
	}
}

static void aice_unpack_dthmb(uint8_t *cmd_ack_code, uint8_t *target_id, uint8_t *extra_word_length)
{
	*cmd_ack_code = usb_in_buffer[0];
	*target_id = usb_in_buffer[1];
	*extra_word_length = usb_in_buffer[2];
}

/***************************************************************************/
/* End of AICE commands' pack/unpack functions */

/* calls the given usb_bulk_* function, allowing for the data to
 * trickle in with some timeouts  */
static int usb_bulk_with_retries(
		int (*f)(usb_dev_handle *, int, char *, int, int),
		usb_dev_handle *dev, int ep,
		char *bytes, int size, int timeout)
{
	int tries = 3, count = 0;

	while (tries && (count < size)) {
		int result = f(dev, ep, bytes + count, size - count, timeout);
		if (result > 0)
			count += result;
#ifndef __MINGW32__
		else if ((-ETIMEDOUT != result) || !--tries)
			return result;
#else
		else if (!--tries)
			return result;
#endif
	}
	return count;
}

static int wrap_usb_bulk_write(usb_dev_handle *dev, int ep,
		char *buff, int size, int timeout)
{
	/* usb_bulk_write() takes const char *buff */
	return usb_bulk_write (dev, ep, buff, size, timeout);
}

static inline int usb_bulk_write_ex(usb_dev_handle *dev, int ep,
		char *bytes, int size, int timeout)
{
	return usb_bulk_with_retries(&wrap_usb_bulk_write,
			dev, ep, bytes, size, timeout);
}

static inline int usb_bulk_read_ex(usb_dev_handle *dev, int ep,
		char *bytes, int size, int timeout)
{
	return usb_bulk_with_retries(&usb_bulk_read,
			dev, ep, bytes, size, timeout);
}

/* Write data from out_buffer to USB. */
static int aice_usb_write(uint8_t *out_buffer, int out_length)
{
	int result;

	if (out_length > AICE_OUT_BUFFER_SIZE) {
		//LOG_ERROR("aice_write illegal out_length=%d (max=%d)",
			//	out_length, AICE_OUT_BUFFER_SIZE);
		return -1;
	}

	result = usb_bulk_write_ex(aice_handler.usb_handle, aice_handler.usb_write_ep,
		(char *)out_buffer, out_length, AICE_USB_TIMEOUT);

	//DEBUG_JTAG_IO("aice_usb_write, out_length = %d, result = %d",
	//		out_length, result);

	return result;
}

/* Read data from USB into in_buffer. */
static int aice_usb_read(uint8_t *in_buffer, int expected_size)
{
	int result = usb_bulk_read_ex(aice_handler.usb_handle, aice_handler.usb_read_ep,
		(char *)in_buffer, expected_size, AICE_USB_TIMEOUT);

	//DEBUG_JTAG_IO("aice_usb_read, result = %d", result);

	return result;
}

/***************************************************************************/
/* AICE commands */
int aice_scan_chain(uint32_t *id_codes, uint8_t *num_of_ids)
{
	int result;

	aice_pack_htda(AICE_CMD_SCAN_CHAIN, 0x0F, 0x0);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDA);

	log_add (LOG_DEBUG, "\tSCAN_CHAIN, length: 0x0F\n");

	/** TODO: modify receive length */
	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHA);
	if (AICE_FORMAT_DTHA != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHA, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	aice_unpack_dtha_multiple_data(&cmd_ack_code, num_of_ids, id_codes, 0x10);

	log_add (LOG_DEBUG, "\tSCAN_CHAIN response, # of IDs: %d\n", *num_of_ids);

	if (cmd_ack_code != AICE_CMD_SCAN_CHAIN) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_SCAN_CHAIN, cmd_ack_code);
		return ERROR_FAIL;
	}

	if (*num_of_ids == 0xFF) {
		log_add (LOG_DEBUG, "\tNo target connected\n");
		return ERROR_FAIL;
	}
	else if (*num_of_ids == 0x10) {
		log_add (LOG_DEBUG, "\tThe ice chain over 16 targets\n");
	}
	else
		(*num_of_ids)++;

	return ERROR_OK;
}

int aice_read_ctrl(uint32_t address, uint32_t *data)
{
	int result;

	aice_pack_htda(AICE_CMD_READ_CTRL, 0, address);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDA);

	log_add (LOG_DEBUG, "\tREAD_CTRL, address: 0x%08x\n", address);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHA);
	if (AICE_FORMAT_DTHA != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHA, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	aice_unpack_dtha(&cmd_ack_code, &extra_length, data);

	log_add (LOG_DEBUG, "\tREAD_CTRL response, data: 0x%08x\n", *data);

	if (cmd_ack_code != AICE_CMD_READ_CTRL) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_READ_CTRL, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_write_ctrl(uint32_t address, uint32_t data)
{
	int result;
	
	aice_pack_htdc(AICE_CMD_WRITE_CTRL, 0, address, data);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDC);

	log_add (LOG_DEBUG, "\tWRITE_CTRL, address: 0x%08x, data: 0x%08x\n", address, data);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHB);
	if (AICE_FORMAT_DTHB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	aice_unpack_dthb(&cmd_ack_code, &extra_length);

	log_add (LOG_DEBUG, "\tWRITE_CTRL response\n");

	if (cmd_ack_code != AICE_CMD_WRITE_CTRL) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_WRITE_CTRL, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_read_dtr(uint8_t target_id, uint32_t *data)
{
	int result;

	aice_pack_htdma(AICE_CMD_T_READ_DTR, target_id, 0, 0);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMA);

	log_add (LOG_DEBUG, "\tREAD_DTR\n");

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMA);
	if (AICE_FORMAT_DTHMA != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMA, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthma(&cmd_ack_code, &res_target_id, &extra_length, data);

	log_add (LOG_DEBUG, "\tREAD_DTR response, data: 0x%08x\n", *data);

	if (cmd_ack_code != AICE_CMD_T_READ_DTR) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_READ_DTR, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_write_dtr(uint8_t target_id, uint32_t data)
{
	int result;
	
	aice_pack_htdmc(AICE_CMD_T_WRITE_DTR, target_id, 0, 0, data);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMC);

	log_add (LOG_DEBUG, "\tWRITE_DTR, data: 0x%08x\n", data);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMB);
	if (AICE_FORMAT_DTHMB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthmb(&cmd_ack_code, &res_target_id, &extra_length);

	log_add (LOG_DEBUG, "\tWRITE_DTR response\n");

	if (cmd_ack_code != AICE_CMD_T_WRITE_DTR) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_WRITE_DTR, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_read_misc(uint8_t target_id, uint32_t address, uint32_t *data)
{
	int result;

	aice_pack_htdma(AICE_CMD_T_READ_MISC, target_id, 0, address);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMA);

	log_add (LOG_DEBUG, "\tREAD_MISC, address: 0x%08x\n", address);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMA);
	if (AICE_FORMAT_DTHMA != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMA, result);
		return ERROR_DISCONNECT;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthma(&cmd_ack_code, &res_target_id, &extra_length, data);

	log_add (LOG_DEBUG, "\tREAD_MISC response, data: 0x%08x\n", *data);

	if (cmd_ack_code != AICE_CMD_T_READ_MISC) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_READ_MISC, cmd_ack_code);
		return ERROR_AICE_TIMEOUT;
	}

	return ERROR_OK;
}

int aice_write_misc(uint8_t target_id, uint32_t address, uint32_t data)
{
	int result;

	aice_pack_htdmc(AICE_CMD_T_WRITE_MISC, target_id, 0, address, data);
	
	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMC);

	log_add (LOG_DEBUG, "\tWRITE_MISC, address: 0x%08x, data: 0x%08x\n", address, data);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMB);
	if (AICE_FORMAT_DTHMB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthmb(&cmd_ack_code, &res_target_id, &extra_length);

	log_add (LOG_DEBUG, "\tWRITE_MISC response\n");

	if (cmd_ack_code != AICE_CMD_T_WRITE_MISC) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_WRITE_MISC, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_read_edmsr(uint8_t target_id, uint32_t address, uint32_t *data)
{
	int result;

	aice_pack_htdma(AICE_CMD_T_READ_EDMSR, target_id, 0, address);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMA);

	log_add (LOG_DEBUG, "\tREAD_EDMSR, address: 0x%08x\n", address);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMA);
	if (AICE_FORMAT_DTHMA != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMA, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthma(&cmd_ack_code, &res_target_id, &extra_length, data);

	log_add (LOG_DEBUG, "\tREAD_EDMSR response, data: 0x%08x\n", *data);

	if (cmd_ack_code != AICE_CMD_T_READ_EDMSR) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_READ_EDMSR, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_write_edmsr(uint8_t target_id, uint32_t address, uint32_t data)
{
	int result;

	aice_pack_htdmc(AICE_CMD_T_WRITE_EDMSR, target_id, 0, address, data);
	
	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMC);

	log_add (LOG_DEBUG, "\tWRITE_EDMSR, address: 0x%08x, data: 0x%08x\n", address, data);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMB);
	if (AICE_FORMAT_DTHMB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthmb(&cmd_ack_code, &res_target_id, &extra_length);

	log_add (LOG_DEBUG, "\tWRITE_EDMSR response\n");

	if (cmd_ack_code != AICE_CMD_T_WRITE_EDMSR) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_WRITE_EDMSR, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static int aice_switch_to_big_endian (uint32_t *word, uint8_t num_of_words)
{
	uint32_t tmp;
	uint8_t i;

	for (i = 0 ; i < num_of_words ; i++)
	{
		tmp = ((word[i] >> 24) & 0x000000FF) |
		      ((word[i] >>  8) & 0x0000FF00) |
		      ((word[i] <<  8) & 0x00FF0000) |
		      ((word[i] << 24) & 0xFF000000);
		word[i] = tmp;
	}

	return ERROR_OK;
}

int aice_write_dim(uint8_t target_id, uint32_t *word, uint8_t num_of_words)
{
	int result;
	uint32_t big_endian_word[4];

	memcpy (big_endian_word, word, sizeof (big_endian_word));

	/** instruction is big-endian */
	aice_switch_to_big_endian (big_endian_word, num_of_words);

	aice_pack_htdmc_multiple_data(AICE_CMD_T_WRITE_DIM, target_id, num_of_words - 1, 0, big_endian_word, num_of_words);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMC + (num_of_words - 1) * 4);

	log_add (LOG_DEBUG, "\tWRITE_DIM, data: 0x%08x, 0x%08x, 0x%08x, 0x%08x\n",
			big_endian_word[0], big_endian_word[1], big_endian_word[2], big_endian_word[3]);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMB);
	if (AICE_FORMAT_DTHMB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthmb(&cmd_ack_code, &res_target_id, &extra_length);

	log_add (LOG_DEBUG, "\tWRITE_DIM response\n");

	if (cmd_ack_code != AICE_CMD_T_WRITE_DIM) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_WRITE_DIM, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_execute(uint8_t target_id)
{
	int result;

	aice_pack_htdmc(AICE_CMD_T_EXECUTE, target_id, 0, 0, 0);
	
	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMC);

	log_add (LOG_DEBUG, "\tEXECUTE\n");

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMB);
	if (AICE_FORMAT_DTHMB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthmb(&cmd_ack_code, &res_target_id, &extra_length);

	log_add (LOG_DEBUG, "\tEXECUTE response\n");

	if (cmd_ack_code != AICE_CMD_T_EXECUTE) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_EXECUTE, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_write_mem_b(uint8_t target_id, uint32_t address, uint32_t data)
{
	int result;

	aice_pack_htdmd(AICE_CMD_T_WRITE_MEM_B, target_id, 0, address, data & 0x000000FF);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMD);

	log_add (LOG_DEBUG, "\tWRITE_MEM_B, ADDRESS 0x%08x  VALUE 0x%08x\n", address, data);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMB);
	if (AICE_FORMAT_DTHMB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthmb(&cmd_ack_code, &res_target_id, &extra_length);

	if (cmd_ack_code != AICE_CMD_T_WRITE_MEM_B) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_WRITE_MEM_B, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_write_mem_h(uint8_t target_id, uint32_t address, uint32_t data)
{
	int result;

	aice_pack_htdmd(AICE_CMD_T_WRITE_MEM_H, target_id, 0, (address >> 1) & 0x7FFFFFFF, data & 0x0000FFFF);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMD);

	log_add (LOG_DEBUG, "\tWRITE_MEM_H, ADDRESS 0x%08x VALUE 0x%08x\n", address, data);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMB);
	if (AICE_FORMAT_DTHMB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthmb(&cmd_ack_code, &res_target_id, &extra_length);

	if (cmd_ack_code != AICE_CMD_T_WRITE_MEM_H) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_WRITE_MEM_H, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_write_mem(uint8_t target_id, uint32_t address, uint32_t data)
{
	int result;

	aice_pack_htdmd(AICE_CMD_T_WRITE_MEM, target_id, 0, (address >> 2) & 0x3FFFFFFF, data);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMD);

	log_add (LOG_DEBUG, "\tWRITE_MEM, ADDRESS 0x%08x VALUE 0x%08x\n", address, data);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMB);
	if (AICE_FORMAT_DTHMB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthmb(&cmd_ack_code, &res_target_id, &extra_length);

	if (cmd_ack_code != AICE_CMD_T_WRITE_MEM) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_WRITE_MEM, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_fastread_mem(uint8_t target_id, uint32_t *word, uint32_t num_of_words)
{
	int result;

	aice_pack_htdmb(AICE_CMD_T_FASTREAD_MEM, target_id, num_of_words - 1, 0);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMB);

	log_add (LOG_DEBUG, "\tFASTREAD_MEM, # of DATA 0x%08x\n", num_of_words);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMA + (num_of_words - 1) * 4);
	if (result < 0) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMA, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthma_multiple_data(&cmd_ack_code, &res_target_id, &extra_length, word);

	if (cmd_ack_code != AICE_CMD_T_FASTREAD_MEM) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_FASTREAD_MEM, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_fastwrite_mem(uint8_t target_id, const uint32_t *word, uint32_t num_of_words)
{
	int result;

	aice_pack_htdmd_multiple_data(AICE_CMD_T_FASTWRITE_MEM, target_id, num_of_words - 1, 0, word);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMD + (num_of_words - 1) * 4);

	log_add (LOG_DEBUG, "\tFASTWRITE_MEM, # of DATA 0x%08x\n", num_of_words);

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMB);
	if (AICE_FORMAT_DTHMB != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMB, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthmb(&cmd_ack_code, &res_target_id, &extra_length);

	if (cmd_ack_code != AICE_CMD_T_FASTWRITE_MEM) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_FASTWRITE_MEM, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_read_mem_b(uint8_t target_id, uint32_t address, uint32_t *data)
{
	int result;

	aice_pack_htdmb(AICE_CMD_T_READ_MEM_B, target_id, 0, address);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMB);

	log_add (LOG_DEBUG, "\tREAD_MEM_B\n");

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMA);
	if (AICE_FORMAT_DTHMA != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMA, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthma(&cmd_ack_code, &res_target_id, &extra_length, data);

	log_add (LOG_DEBUG, "\tREAD_MEM_B response, data: 0x%08x\n", *data);

	if (cmd_ack_code != AICE_CMD_T_READ_MEM_B) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_READ_MEM_B, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_read_mem_h(uint8_t target_id, uint32_t address, uint32_t *data)
{
	int result;

	aice_pack_htdmb(AICE_CMD_T_READ_MEM_H, target_id, 0, (address >> 1) & 0x7FFFFFFF);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMB);

	log_add (LOG_DEBUG, "\tREAD_MEM_H\n");

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMA);
	if (AICE_FORMAT_DTHMA != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMA, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthma(&cmd_ack_code, &res_target_id, &extra_length, data);

	log_add (LOG_DEBUG, "\tREAD_MEM_H response, data: 0x%08x\n", *data);

	if (cmd_ack_code != AICE_CMD_T_READ_MEM_H) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_READ_MEM_H, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

int aice_read_mem(uint8_t target_id, uint32_t address, uint32_t *data)
{
	int result;

	aice_pack_htdmb(AICE_CMD_T_READ_MEM, target_id, 0, (address >> 2) & 0x3FFFFFFF);

	aice_usb_write(usb_out_buffer, AICE_FORMAT_HTDMB);

	log_add (LOG_DEBUG, "\tREAD_MEM\n");

	result = aice_usb_read(usb_in_buffer, AICE_FORMAT_DTHMA);
	if (AICE_FORMAT_DTHMA != result) {
		log_add (LOG_DEBUG, "\taice_usb_read failed (requested=%d, result=%d)\n", AICE_FORMAT_DTHMA, result);
		return ERROR_FAIL;
	}

	uint8_t cmd_ack_code;
	uint8_t extra_length;
	uint8_t res_target_id;
	aice_unpack_dthma(&cmd_ack_code, &res_target_id, &extra_length, data);

	log_add (LOG_DEBUG, "\tREAD_MEM response, data: 0x%08x\n", *data);

	if (cmd_ack_code != AICE_CMD_T_READ_MEM) {
		log_add (LOG_DEBUG, "\taice command timeout (command=0x%x, response=0x%x)\n", AICE_CMD_T_READ_MEM, cmd_ack_code);
		return ERROR_FAIL;
	}

	return ERROR_OK;
}

/***************************************************************************/
/* End of AICE commands */

static int libusb_open (uint16_t vid, uint16_t pid, struct usb_dev_handle **handle)
{
	struct usb_bus *bus;
	struct usb_device *dev;
	struct usb_bus *busses;

	usb_init ();
	usb_find_busses ();
	usb_find_devices ();

	busses = usb_get_busses ();
	for (bus = busses ; bus ; bus = bus->next)
	{
		for (dev = bus->devices ; dev ; dev = dev->next)
		{
			if ((dev->descriptor.idVendor == vid) && (dev->descriptor.idProduct == pid))
			{
				*handle = usb_open (dev);
				if (*handle == NULL)
					return -errno;

				return 0;
			}
		}
	}

	return -ENODEV;
}

static int libusb_get_endpoints(struct usb_device *udev,
		uint32_t *usb_read_ep,
		uint32_t *usb_write_ep)
{
	struct usb_interface *iface = udev->config->interface;
	struct usb_interface_descriptor *desc = iface->altsetting;
	int i;

	for (i = 0; i < desc->bNumEndpoints; i++) {
		uint8_t epnum = desc->endpoint[i].bEndpointAddress;
		bool is_input = epnum & 0x80;
		if (is_input)
			*usb_read_ep = epnum;
		else
			*usb_write_ep = epnum;
	}

	return 0;
}

int aice_usb_open(uint16_t vid, uint16_t pid)
{
	struct usb_dev_handle *devh;

	if (libusb_open(vid, pid, &devh) != ERROR_OK)
		return ERROR_FAIL;

	/* BE ***VERY CAREFUL*** ABOUT MAKING CHANGES IN THIS
	 * AREA!!!!!!!!!!!  The behavior of libusb is not completely
	 * consistent across Windows, Linux, and Mac OS X platforms.
	 * The actions taken in the following compiler conditionals may
	 * not agree with published documentation for libusb, but were
	 * found to be necessary through trials and tribulations.  Even
	 * little tweaks can break one or more platforms, so if you do
	 * make changes test them carefully on all platforms before
	 * committing them!
	 */

#ifndef __MINGW32__
	usb_reset (devh);

	int timeout = 5;
	/* reopen jlink after usb_reset
	 * on win32 this may take a second or two to re-enumerate */
	int retval;
	while ((retval = libusb_open(vid, pid, &devh)) != ERROR_OK) {
		usleep(1000);
		timeout--;
		if (!timeout)
			break;
	}
	if (ERROR_OK != retval)
		return ERROR_FAIL;
#endif

	/* usb_set_configuration required under win32 */
	struct usb_device *udev = usb_device(devh);
	usb_set_configuration (devh, udev->config[0].bConfigurationValue);
	usb_claim_interface (devh, 0);

	uint32_t aice_read_ep;
	uint32_t aice_write_ep;
	libusb_get_endpoints(udev, &aice_read_ep, &aice_write_ep);

	aice_handler.usb_read_ep = aice_read_ep;
	aice_handler.usb_write_ep = aice_write_ep;
	aice_handler.usb_handle = devh;

	return ERROR_OK;
}

int aice_usb_close(void)
{
	usb_close (aice_handler.usb_handle);

	return ERROR_OK;
}
