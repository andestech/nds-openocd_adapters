#include "config.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>
#include "aice_port.h"
#include "nds32_edm.h"
#include "nds32_reg.h"
#include "nds32_insn.h"
#include "aice_usb.h"
#include "log.h"

#define MAXLINE 8192

static const char *AICE_MEMORY_ACCESS_NAME[] = {
	"BUS",
	"CPU",
};

static const char *AICE_MEMORY_MODE_NAME[] = {
	"AUTO",
	"MEM",
	"ILM",
	"DLM",
};

static uint32_t jtag_clock;
static uint8_t current_target_id = 0;
static uint32_t r0_backup;
static uint32_t r1_backup;
static enum aice_target_state_s core_state = AICE_TARGET_RUNNING;
static enum aice_memory_access access_channel = AICE_MEMORY_ACC_CPU;
static enum aice_memory_mode memory_mode = AICE_MEMORY_MODE_AUTO;
static bool memory_mode_auto_select;
static uint32_t edm_version;

static struct cache_info icache = {0, 0, 0, 0, 0};
static struct cache_info dcache = {0, 0, 0, 0, 0};
static bool cache_init = false;

static int aice_usb_read_reg(uint32_t num, uint32_t *val);
static int aice_usb_write_reg(uint32_t num, uint32_t val);

static int backup_tmp_registers(void)
{
	log_add (LOG_DEBUG, "--<backup_tmp_registers>\n");

	aice_usb_read_reg(R0, &r0_backup);
	aice_usb_read_reg(R1, &r1_backup);

	log_add (LOG_DEBUG, "\tr0: 0x%08x, r1: 0x%08x\n", r0_backup, r1_backup);

	return ERROR_OK;
}

static int restore_tmp_registers(void)
{
	log_add (LOG_DEBUG, "--<restore_tmp_registers> - r0: 0x%08x, r1: 0x%08x\n", r0_backup, r1_backup);

	aice_usb_write_reg(R0, r0_backup);
	aice_usb_write_reg(R1, r1_backup);

	return ERROR_OK;
}

static int aice_get_version_info(void)
{
	log_add (LOG_DEBUG, "--<aice_get_version_info>\n");

	uint32_t hardware_version;
	uint32_t firmware_version;
	uint32_t fpga_version;

	if (aice_read_ctrl(AICE_READ_CTRL_GET_HARDWARE_VERSION, &hardware_version) != ERROR_OK)
		return ERROR_FAIL;

	if (aice_read_ctrl(AICE_READ_CTRL_GET_FIRMWARE_VERSION, &firmware_version) != ERROR_OK)
		return ERROR_FAIL;

	if (aice_read_ctrl(AICE_READ_CTRL_GET_FPGA_VERSION, &fpga_version) != ERROR_OK)
		return ERROR_FAIL;

	return ERROR_OK;
}

static int aice_edm_reset(void)
{
	log_add (LOG_DEBUG, "--<aice_edm_reset>\n");

	// Clear timeout
	if (aice_write_ctrl(AICE_WRITE_CTRL_CLEAR_TIMEOUT_STATUS, 0x1) != ERROR_OK)
		return ERROR_FAIL;
	
	// TRST
	if (aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_CONTROL, AICE_JTAG_PIN_CONTROL_TRST) != ERROR_OK)
		return ERROR_FAIL;
	
	// TCK_SCAN
	if (aice_write_ctrl(AICE_WRITE_CTRL_TCK_CONTROL, AICE_TCK_CONTROL_TCK_SCAN) != ERROR_OK)
		return ERROR_FAIL;
	
	return ERROR_OK;
}

static int aice_usb_set_clock(int speed)
{
	log_add (LOG_DEBUG, "--<aice_usb_set_clock>, clock: 0x%x\n", speed);

	// Set JTAG clock
	if (aice_write_ctrl(AICE_WRITE_CTRL_TCK_CONTROL, speed) != ERROR_OK)
		return ERROR_FAIL;

	uint32_t check_speed;
	// Read back JTAG clock
	if (aice_read_ctrl(AICE_READ_CTRL_GET_ICE_STATE, &check_speed) != ERROR_OK)
		return ERROR_FAIL;

	if (((int)check_speed & 0x0F) != speed)
		return ERROR_FAIL;
	
	return ERROR_OK;
}

static int aice_edm_init(void)
{
	log_add (LOG_DEBUG, "--<aice_edm_init>\n");

	aice_write_edmsr(current_target_id, NDS_EDM_SR_DIMBR, 0xFFFF0000);
	aice_write_edmsr(current_target_id, NDS_EDM_SR_EDM_CTL, 0xA000004F);
	aice_write_misc(current_target_id, NDS_EDM_MISC_DBGER, NDS_DBGER_DEX|NDS_DBGER_DPED|NDS_DBGER_CRST|NDS_DBGER_AT_MAX);
	aice_write_misc(current_target_id, NDS_EDM_MISC_DIMIR, 0);

	/* get EDM version */
	uint32_t value_edmcfg;
	aice_read_edmsr(current_target_id, NDS_EDM_SR_EDM_CFG, &value_edmcfg);
	edm_version = (value_edmcfg >> 16) & 0xFFFF;

	return ERROR_OK;
}

static int aice_usb_reset(void)
{
	log_add (LOG_DEBUG, "--<aice_usb_reset>\n");

	aice_edm_reset ();

	if (aice_usb_set_clock (jtag_clock) != ERROR_OK)
		return ERROR_FAIL;

	return ERROR_OK;
}

static int aice_issue_srst(void)
{
	log_add (LOG_DEBUG, "--<aice_issue_srst>\n");

	// issue srst
	if (aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_CONTROL, AICE_JTAG_PIN_CONTROL_SRST) != ERROR_OK)
		return ERROR_FAIL;

	uint32_t dbger_value;
	if (aice_read_misc(current_target_id, NDS_EDM_MISC_DBGER, &dbger_value) != ERROR_OK)
		return ERROR_FAIL;

	if (dbger_value & NDS_DBGER_CRST)
	{
		core_state = AICE_TARGET_RUNNING;
		return ERROR_OK;
	}
	return ERROR_FAIL;
}

static int aice_issue_reset_hold(void)
{
	log_add (LOG_DEBUG, "--<aice_issue_reset_hold>\n");

	// set no_dbgi_pin to 0
	uint32_t pin_status;
	aice_read_ctrl(AICE_READ_CTRL_GET_JTAG_PIN_STATUS, &pin_status);
	if (pin_status | 0x4)
	{
		aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_STATUS, pin_status & (~0x4));
	}

	// issue restart
	if (aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_CONTROL, AICE_JTAG_PIN_CONTROL_RESTART) != ERROR_OK)
		return ERROR_FAIL;

	uint32_t dbger_value;
	if (aice_read_misc(current_target_id, NDS_EDM_MISC_DBGER, &dbger_value) != ERROR_OK)
		return ERROR_FAIL;

	if ((NDS_DBGER_CRST | NDS_DBGER_DEX) == (dbger_value & (NDS_DBGER_CRST | NDS_DBGER_DEX)))
	{
		/* backup r0 & r1 */
		backup_tmp_registers();
		core_state = AICE_TARGET_HALTED;

		return ERROR_OK;
	}
	else
	{
		// set no_dbgi_pin to 1
		aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_STATUS, pin_status | 0x4);

		// issue restart again
		if (aice_write_ctrl(AICE_WRITE_CTRL_JTAG_PIN_CONTROL, AICE_JTAG_PIN_CONTROL_RESTART) != ERROR_OK)
			return ERROR_FAIL;

		if (aice_read_misc(current_target_id, NDS_EDM_MISC_DBGER, &dbger_value) != ERROR_OK)
			return ERROR_FAIL;

		if ((NDS_DBGER_CRST | NDS_DBGER_DEX) == (dbger_value & (NDS_DBGER_CRST | NDS_DBGER_DEX)))
		{
			/* backup r0 & r1 */
			backup_tmp_registers();
			core_state = AICE_TARGET_HALTED;

			return ERROR_OK;
		}

		// TODO: do software reset-and-hold
	}

	return ERROR_FAIL;
}

static int aice_usb_assert_srst(enum aice_srst_type_s srst)
{
	log_add (LOG_DEBUG, "--<aice_usb_assert_srst>\n");

	if ((AICE_SRST != srst) && (AICE_RESET_HOLD != srst))
		return ERROR_FAIL;

	// clear DBGER
	if (aice_write_misc(current_target_id, NDS_EDM_MISC_DBGER, NDS_DBGER_DEX|NDS_DBGER_DPED|NDS_DBGER_CRST) != ERROR_OK)
		return ERROR_FAIL;

	if (AICE_SRST == srst)
		return aice_issue_srst();
	else
		return aice_issue_reset_hold();

	return ERROR_OK;
}

static int check_suppressed_exception(uint32_t dbger_value)
{
	uint32_t ir4_value;
	uint32_t ir6_value;

	if ((dbger_value & NDS_DBGER_ALL_SUPRS_EX) == NDS_DBGER_ALL_SUPRS_EX)
	{
		log_add (LOG_INFO, "Exception is detected and suppressed\n");

		aice_usb_read_reg(IR4, &ir4_value);
		/* Clear IR6.SUPRS_EXC, IR6.IMP_EXC */
		aice_usb_read_reg(IR6, &ir6_value);
		/*
		 * For MCU version(MSC_CFG.MCU == 1) like V3m
		 *  | SWID[30:16] | Reserved[15:10] | SUPRS_EXC[9]  | IMP_EXC[8]  | VECTOR[7:5]  | INST[4] | Exc Type[3:0] | 
		 *
		 * For non-MCU version(MSC_CFG.MCU == 0) like V3
		 *  | SWID[30:16] | Reserved[15:14] | SUPRS_EXC[13] | IMP_EXC[12] | VECTOR[11:5] | INST[4] | Exc Type[3:0] | 
		 */
		log_add (LOG_INFO, "EVA: 0x%08x\n", ir4_value);
		log_add (LOG_INFO, "ITYPE: 0x%08x\n", ir6_value);

		ir6_value = ir6_value & (~0x300); /* for MCU */
		ir6_value = ir6_value & (~0x3000); /* for non-MCU */
		aice_usb_write_reg(IR6, ir6_value);

		return ERROR_FAIL;
	}

	return ERROR_OK;
}

static int aice_execute_dim(uint32_t *insts, uint8_t n_inst)
{
	log_add (LOG_DEBUG, "--<aice_execute_dim>\n");

	/** fill DIM */
	if (aice_write_dim(current_target_id, insts, n_inst) != ERROR_OK)
		return ERROR_FAIL;

	/** clear DBGER.DPED */
	if (aice_write_misc(current_target_id, NDS_EDM_MISC_DBGER, NDS_DBGER_DPED) != ERROR_OK)
		return ERROR_FAIL;

	/** execute DIM */
	if (aice_do_execute(current_target_id) != ERROR_OK)
		return ERROR_FAIL;

	/** read DBGER.DPED */
	uint32_t dbger_value;
	if (aice_read_misc(current_target_id, NDS_EDM_MISC_DBGER, &dbger_value) != ERROR_OK)
		return ERROR_FAIL;

	if ((dbger_value & NDS_DBGER_DPED) != NDS_DBGER_DPED)
	{
		log_add (LOG_INFO, "Debug operations do not finish properly: \
				0x%08x 0x%08x 0x%08x 0x%08x\n", insts[0], insts[1], insts[2], insts[3]);
		return ERROR_FAIL;
	}

	if (ERROR_OK != check_suppressed_exception(dbger_value))
		return ERROR_FAIL;

	return ERROR_OK;
}

static int aice_usb_read_reg(uint32_t num, uint32_t *val)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_reg>, reg_no: 0x%08x\n", num);

	uint32_t instructions[4]; /** execute instructions in DIM */

	if (NDS32_REG_TYPE_GPR == nds32_reg_type(num))  // general registers
	{
		instructions[0] = MTSR_DTR(num);
		instructions[1] = DSB;
		instructions[2] = NOP;
		instructions[3] = BEQ_MINUS_12;
	}
	else if (NDS32_REG_TYPE_SPR == nds32_reg_type(num)) // user special registers
	{
		instructions[0] = MFUSR_G0(0, nds32_reg_sr_index(num));
		instructions[1] = MTSR_DTR(0);
		instructions[2] = DSB;
		instructions[3] = BEQ_MINUS_12;
	}
	else if (NDS32_REG_TYPE_AUMR == nds32_reg_type(num)) // audio registers
	{
		if ((CB_CTL <= num) && (num <= CBE3))
		{
			instructions[0] = AMFAR2(0, nds32_reg_sr_index(num));
			instructions[1] = MTSR_DTR(0);
			instructions[2] = DSB;
			instructions[3] = BEQ_MINUS_12;
		}
		else
		{
			instructions[0] = AMFAR(0, nds32_reg_sr_index(num));
			instructions[1] = MTSR_DTR(0);
			instructions[2] = DSB;
			instructions[3] = BEQ_MINUS_12;
		}
	}
	else if (NDS32_REG_TYPE_FPU == nds32_reg_type(num)) // fpu registers
	{
		if (FPCSR == num)
		{
			instructions[0] = FMFCSR;
			instructions[1] = MTSR_DTR(0);
			instructions[2] = DSB;
			instructions[3] = BEQ_MINUS_12;
		}
		else if (FPCFG == num)
		{
			instructions[0] = FMFCFG;
			instructions[1] = MTSR_DTR(0);
			instructions[2] = DSB;
			instructions[3] = BEQ_MINUS_12;
		}
		else
		{
			if (FS0 <= num && num <= FS31) // single precision
			{
				instructions[0] = FMFSR(0, nds32_reg_sr_index(num));
				instructions[1] = MTSR_DTR(0);
				instructions[2] = DSB;
				instructions[3] = BEQ_MINUS_12;
			}
			else if (FD0 <= num && num <= FD31) // double precision
			{
				instructions[0] = FMFDR(0, nds32_reg_sr_index(num));
				instructions[1] = MTSR_DTR(0);
				instructions[2] = DSB;
				instructions[3] = BEQ_MINUS_12;
			}
		}
	}
	else  // system registers
	{
		instructions[0] = MFSR(0, nds32_reg_sr_index(num));
		instructions[1] = MTSR_DTR(0);
		instructions[2] = DSB;
		instructions[3] = BEQ_MINUS_12;
	}

	aice_execute_dim (instructions, 4);

	uint32_t value_edmsw;
	aice_read_edmsr (current_target_id, NDS_EDM_SR_EDMSW, &value_edmsw);
	if (value_edmsw & NDS_EDMSW_WDV)
		aice_read_dtr (current_target_id, val);
	else
		return ERROR_FAIL;

	return ERROR_OK;
}

static int aice_usb_write_reg(uint32_t num, uint32_t val)
{
	log_add (LOG_DEBUG, "--<aice_usb_write_reg>, reg_no: 0x%08x, value: 0x%08x\n", num, val);

	uint32_t instructions[4]; /** execute instructions in DIM */
	uint32_t value_edmsw;

	aice_write_dtr (current_target_id, val);
	aice_read_edmsr (current_target_id, NDS_EDM_SR_EDMSW, &value_edmsw);
	if (0 == (value_edmsw & NDS_EDMSW_RDV))
		return ERROR_FAIL;

	if (NDS32_REG_TYPE_GPR == nds32_reg_type(num))  // general registers
	{
		instructions[0] = MFSR_DTR(num);
		instructions[1] = DSB;
		instructions[2] = NOP;
		instructions[3] = BEQ_MINUS_12;
	}
	else if (NDS32_REG_TYPE_SPR == nds32_reg_type(num)) // user special registers
	{
		instructions[0] = MFSR_DTR(0);
		instructions[1] = MTUSR_G0(0, nds32_reg_sr_index(num));
		instructions[2] = DSB;
		instructions[3] = BEQ_MINUS_12;
	}
	else if (NDS32_REG_TYPE_AUMR == nds32_reg_type(num)) // audio registers
	{
		if ((CB_CTL <= num) && (num <= CBE3))
		{
			instructions[0] = MFSR_DTR(0);
			instructions[1] = AMTAR2(0, nds32_reg_sr_index(num));
			instructions[2] = DSB;
			instructions[3] = BEQ_MINUS_12;
		}
		else
		{
			instructions[0] = MFSR_DTR(0);
			instructions[1] = AMTAR(0, nds32_reg_sr_index(num));
			instructions[2] = DSB;
			instructions[3] = BEQ_MINUS_12;
		}
	}
	else if (NDS32_REG_TYPE_FPU == nds32_reg_type(num)) // fpu registers
	{
		if (FPCSR == num)
		{
			instructions[0] = MFSR_DTR(0);
			instructions[1] = FMTCSR;
			instructions[2] = DSB;
			instructions[3] = BEQ_MINUS_12;
		}
		else if (FPCFG == num)
		{
			/* FPCFG is readonly */
		}
		else
		{
			if (FS0 <= num && num <= FS31) // single precision
			{
				instructions[0] = MFSR_DTR(0);
				instructions[1] = FMTSR(0, nds32_reg_sr_index(num));
				instructions[2] = DSB;
				instructions[3] = BEQ_MINUS_12;
			}
			else if (FD0 <= num && num <= FD31) // double precision
			{
				instructions[0] = MFSR_DTR(0);
				instructions[1] = FMTDR(0, nds32_reg_sr_index(num));
				instructions[2] = DSB;
				instructions[3] = BEQ_MINUS_12;
			}
		}
	}
	else
	{
		instructions[0] = MFSR_DTR(0);
		instructions[1] = MTSR(0, nds32_reg_sr_index(num));
		instructions[2] = DSB;
		instructions[3] = BEQ_MINUS_12;
	}

	return aice_execute_dim (instructions, 4);
}

static int aice_usb_run(void)
{
	log_add (LOG_DEBUG, "--<aice_usb_run>\n");

	uint32_t dbger_value;
	if (aice_read_misc(current_target_id, NDS_EDM_MISC_DBGER, &dbger_value) != ERROR_OK)
		return ERROR_FAIL;

	if ((dbger_value & NDS_DBGER_DEX) != NDS_DBGER_DEX)
	{
		log_add (LOG_INFO, "The debug target unexpectedly exited the debug mode\n");
		return ERROR_FAIL;
	}
	
	/* restore r0 & r1 before free run */
	restore_tmp_registers();
	core_state = AICE_TARGET_RUNNING;

	/* clear DBGER */
	aice_write_misc(current_target_id, NDS_EDM_MISC_DBGER, NDS_DBGER_DEX|NDS_DBGER_DPED|NDS_DBGER_CRST|NDS_DBGER_AT_MAX);

	/** execute instructions in DIM */
	uint32_t instructions[4] = {
		NOP, 
		NOP, 
		NOP, 
		IRET};

	return aice_execute_dim (instructions, 4);
}

static int aice_usb_halt(void)
{
	log_add (LOG_DEBUG, "--<aice_usb_halt>\n");

        /** Clear EDM_CTL.DBGIM & EDM_CTL.DBGACKM */
        uint32_t edm_ctl_value;
        aice_read_edmsr (current_target_id, NDS_EDM_SR_EDM_CTL, &edm_ctl_value);
        if (edm_ctl_value | 0x3)
                aice_write_edmsr (current_target_id, NDS_EDM_SR_EDM_CTL, edm_ctl_value & ~(0x3));

        /** Issue DBGI */
        aice_write_misc(current_target_id, NDS_EDM_MISC_EDM_CMDR, 0);

	int i = 0;
        uint32_t dbger;
	uint32_t acc_ctl_value;
	while (1) {
		aice_read_misc(current_target_id, NDS_EDM_MISC_DBGER, &dbger);

		if (dbger & NDS_DBGER_DEX)
			break;

		if (i >= 30) {
			break;
		}
		i++;
	}

	if (i >= 30)
	{
		/** Try to use FORCE_DBG */
		aice_read_misc (current_target_id, NDS_EDM_MISC_ACC_CTL, &acc_ctl_value);
		acc_ctl_value |= 0x8;
		aice_write_misc (current_target_id, NDS_EDM_MISC_ACC_CTL, acc_ctl_value);

		/** Issue DBGI after enable FORCE_DBG */
		aice_write_misc(current_target_id, NDS_EDM_MISC_EDM_CMDR, 0);

		aice_read_misc(current_target_id, NDS_EDM_MISC_DBGER, &dbger);

		if ((dbger & NDS_DBGER_DEX) == 0)
			return ERROR_FAIL;
	}

        /** set EDM_CTL.DBGIM & EDM_CTL.DBGACKM after halt */
        if (edm_ctl_value | 0x3)
		aice_write_edmsr (current_target_id, NDS_EDM_SR_EDM_CTL, edm_ctl_value);

	/* backup r0 & r1 */
	backup_tmp_registers();
	core_state = AICE_TARGET_HALTED;

        return ERROR_OK;
}

static enum aice_target_state_s aice_usb_state(void)
{
	log_add (LOG_DEBUG, "--<aice_usb_state>\n");

	uint32_t dbger_value;
	uint32_t ice_state;
	int result;

	result = aice_read_misc(current_target_id, NDS_EDM_MISC_DBGER, &dbger_value);
	if (ERROR_AICE_TIMEOUT == result)
	{
		if (aice_read_ctrl(AICE_READ_CTRL_GET_ICE_STATE, &ice_state) != ERROR_OK)
			return AICE_DISCONNECT;

		if ((ice_state & 0x20) == 0)
		{
			log_add (LOG_DEBUG, "\tTarget is disconnected\n");
			return AICE_TARGET_DETACH;
		}
		else
			return AICE_TARGET_UNKNOWN;
	}
	else if (ERROR_DISCONNECT == result)
	{
		log_add (LOG_DEBUG, "\tUSB is disconnected\n");
		return AICE_DISCONNECT;
	}

	if ((dbger_value & NDS_DBGER_DEX) == NDS_DBGER_DEX)
	{
		if (core_state == AICE_TARGET_RUNNING)
		{
			/* backup r0 & r1 */
			backup_tmp_registers();
			core_state = AICE_TARGET_HALTED;
		}

		return AICE_TARGET_HALTED;
	}
	else if ((dbger_value & NDS_DBGER_CRST) == NDS_DBGER_CRST)
	{
		core_state = AICE_TARGET_RUNNING;
		return AICE_TARGET_RESET;
	}
	else if ((dbger_value & NDS_DBGER_AT_MAX) == NDS_DBGER_AT_MAX)
	{
		uint32_t ir11_value;

		/* Clear AT_MAX */
		aice_write_misc(current_target_id, NDS_EDM_MISC_DBGER, NDS_DBGER_AT_MAX);

		/* Issue DBGI to enter debug mode */
		aice_write_misc(current_target_id, NDS_EDM_MISC_EDM_CMDR, 0);

		/* Read OIPC to find out the trigger point */
		aice_usb_read_reg(IR11, &ir11_value);
		log_add (LOG_DEBUG, "stall due to max_stop, trigger point: 0x%08x\n", ir11_value);

		return AICE_TARGET_HALTED;
	}
	else
		return AICE_TARGET_RUNNING;
}

static int aice_usb_step(void)
{
	log_add (LOG_DEBUG, "--<aice_usb_step>\n");

	uint32_t ir0_value;
	uint32_t ir0_reg_num;

	if ((edm_version & 0x1000) == 0)
		ir0_reg_num = IR1;  /* V2 EDM will push interrupt stack as debug exception */
	else
		ir0_reg_num = IR0;

	/** enable HSS */
	aice_usb_read_reg(ir0_reg_num, &ir0_value);
	if ((ir0_value & 0x800) == 0)
	{
		/** set PSW.HSS */
		ir0_value |= (0x01 << 11);
		aice_usb_write_reg(ir0_reg_num, ir0_value);
	}

	if (ERROR_FAIL == aice_usb_run ())
		return ERROR_FAIL;

	int i = 0;
	enum aice_target_state_s state;
	while (1) {
		/* read DBGER */
		state = aice_usb_state ();
		if (AICE_DISCONNECT == state)
			return ERROR_FAIL;
		else if (AICE_TARGET_HALTED == state)
			break;

		if (i >= 30) {
			return ERROR_FAIL;
		}
		i++;
	}

	/** disable HSS */
	aice_usb_read_reg(ir0_reg_num, &ir0_value);
	ir0_value &= ~(0x01 << 11);
	aice_usb_write_reg(ir0_reg_num, ir0_value);

	return ERROR_OK;
}

static int aice_usb_read_debug_reg(uint32_t addr, uint32_t *val)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_debug_reg>, addr: 0x%08x\n", addr);

	return aice_read_edmsr(current_target_id, addr, val);
}

static int aice_usb_write_debug_reg(uint32_t addr, const uint32_t val)
{
	log_add (LOG_DEBUG, "--<aice_usb_write_debug_reg>, addr: 0x%08x, value: 0x%08x\n", addr, val);

	return aice_write_edmsr(current_target_id, addr, val);
}

static int aice_usb_set_address_dim(uint32_t address)
{
	log_add (LOG_DEBUG, "--<aice_usb_set_address_dim>, addr: 0x%08x\n", address);

	uint32_t instructions[4] = {
		SETHI(R0, address >> 12), 
		ORI(R0, R0, address & 0x00000FFF), 
		NOP,
		BEQ_MINUS_12};

	return aice_execute_dim (instructions, 4);
}

static int aice_usb_read_mem_b_dim(uint32_t address, uint32_t *data)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_mem_b_dim>, addr: 0x%08x\n", address);

	uint32_t value;
	uint32_t instructions[4] = {
		LBI_BI(R1, R0),
		MTSR_DTR(R1), 
		DSB,
		BEQ_MINUS_12};

	aice_execute_dim (instructions, 4);

	aice_read_dtr (current_target_id, &value);
	*data = value & 0xFF;

	return ERROR_OK;
}

static int aice_usb_read_mem_h_dim(uint32_t address, uint32_t *data)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_mem_h_dim>, addr: 0x%08x\n", address);

	uint32_t value;
	uint32_t instructions[4] = {
		LHI_BI(R1, R0), 
		MTSR_DTR(R1), 
		DSB,
		BEQ_MINUS_12};

	aice_execute_dim (instructions, 4);

	aice_read_dtr (current_target_id, &value);
	*data = value & 0xFFFF;

	return ERROR_OK;
}

static int aice_usb_read_mem_w_dim(uint32_t address, uint32_t *data)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_mem_w_dim>, addr: 0x%08x\n", address);

	uint32_t instructions[4] = {
		LWI_BI(R1, R0), 
		MTSR_DTR(R1), 
		DSB,
		BEQ_MINUS_12};

	aice_execute_dim (instructions, 4);

	aice_read_dtr (current_target_id, data);

	return ERROR_OK;
}

static int aice_usb_read_mem_b_bus(uint32_t address, uint32_t *data)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_mem_b_bus>, addr: 0x%08x, data: 0x%02x\n", address, *data & 0xFF);

	return aice_read_mem_b(current_target_id, address, data);
}

static int aice_usb_read_mem_h_bus(uint32_t address, uint32_t *data)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_mem_h_bus>, addr: 0x%08x, data: 0x%04x\n", address, *data & 0xFFFF);

	return aice_read_mem_h(current_target_id, address, data);
}

static int aice_usb_read_mem_w_bus(uint32_t address, uint32_t *data)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_mem_w_bus>, addr: 0x%08x, data: 0x%08x\n", address, *data);

	return aice_read_mem(current_target_id, address, data);
}

typedef int (*read_mem_func_t)(uint32_t address, uint32_t *data);

static int aice_usb_read_memory_unit(uint32_t addr, uint32_t size, uint32_t count, uint8_t *buffer)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_memory_unit>, addr: 0x%08x, size: %d, count: 0x%08x\n", addr, size, count);

        uint32_t value;
	size_t i;
	read_mem_func_t read_mem_func;

	switch (size) {
		case 1:
			if (AICE_MEMORY_ACC_BUS == access_channel)
				read_mem_func = aice_usb_read_mem_b_bus;
			else
				read_mem_func = aice_usb_read_mem_b_dim;

			for (i = 0; i < count; i++) {
				read_mem_func (addr, &value);
                                *buffer++ = (uint8_t)value;
                                addr++;
                        }
                        break;
                case 2:
			if (AICE_MEMORY_ACC_BUS == access_channel)
				read_mem_func = aice_usb_read_mem_h_bus;
			else
				read_mem_func = aice_usb_read_mem_h_dim;

			for (i = 0; i < count; i++) {
				read_mem_func (addr, &value);
				uint16_t svalue = value;
				memcpy(buffer, &svalue, sizeof(uint16_t));
                                buffer += 2;
                                addr += 2;
                        }
                        break;
                case 4:
			if (AICE_MEMORY_ACC_BUS == access_channel)
				read_mem_func = aice_usb_read_mem_w_bus;
			else
				read_mem_func = aice_usb_read_mem_w_dim;

			for (i = 0; i < count; i++) {
				read_mem_func (addr, &value);
				memcpy(buffer, &value, sizeof(uint32_t));
                                buffer += 4;
                                addr += 4;
                        }
                        break;
        }

        return ERROR_OK;
}

static int aice_usb_write_mem_b_dim(uint32_t address, uint32_t data)
{
	log_add (LOG_DEBUG, "--<aice_usb_write_mem_b_dim>, addr: 0x%08x, data: 0x%02x\n", address, data & 0xFF);

	uint32_t instructions[4] = {
		MFSR_DTR(R1), 
		SBI_BI(R1, R0),
		DSB,
		BEQ_MINUS_12};

	aice_write_dtr (current_target_id, data & 0xFF);
	aice_execute_dim (instructions, 4);

	return ERROR_OK;
}

static int aice_usb_write_mem_h_dim(uint32_t address, uint32_t data)
{
	log_add (LOG_DEBUG, "--<aice_usb_write_mem_h_dim>, addr: 0x%08x, data: 0x%04x\n", address, data & 0xFFFF);

	uint32_t instructions[4] = {
		MFSR_DTR(R1), 
		SHI_BI(R1, R0), 
		DSB,
		BEQ_MINUS_12};

	aice_write_dtr (current_target_id, data & 0xFFFF);
	aice_execute_dim (instructions, 4);

	return ERROR_OK;
}

static int aice_usb_write_mem_w_dim(uint32_t address, uint32_t data)
{
	log_add (LOG_DEBUG, "--<aice_usb_write_mem_w_dim>, addr: 0x%08x, data: 0x%08x\n", address, data);

	uint32_t instructions[4] = {
		MFSR_DTR(R1), 
		SWI_BI(R1, R0), 
		DSB,
		BEQ_MINUS_12};

	aice_write_dtr (current_target_id, data);
	aice_execute_dim (instructions, 4);

	return ERROR_OK;
}

static int aice_usb_write_mem_b_bus(uint32_t address, uint32_t data)
{
	log_add (LOG_DEBUG, "--<aice_usb_write_mem_b_bus>, addr: 0x%08x, data: 0x%02x\n", address, data & 0xFF);

	return aice_write_mem_b(current_target_id, address, data);
}

static int aice_usb_write_mem_h_bus(uint32_t address, uint32_t data)
{
	log_add (LOG_DEBUG, "--<aice_usb_write_mem_h_bus>, addr: 0x%08x, data: 0x%04x\n", address, data & 0xFFFF);

	return aice_write_mem_h(current_target_id, address, data);
}

static int aice_usb_write_mem_w_bus(uint32_t address, uint32_t data)
{
	log_add (LOG_DEBUG, "--<aice_usb_write_mem_w_bus>, addr: 0x%08x, data: 0x%08x\n", address, data);

	return aice_write_mem(current_target_id, address, data);
}

typedef int (*write_mem_func_t)(uint32_t address, uint32_t data);

static int aice_usb_write_memory_unit(uint32_t addr, uint32_t size, uint32_t count, const uint8_t *buffer)
{
	log_add (LOG_DEBUG, "--<aice_usb_write_memory_unit>, addr: 0x%08x, size: %d, count: 0x%08x\n", addr, size, count);

	size_t i;
	write_mem_func_t write_mem_func;

	switch (size) {
		case 1:
			if (AICE_MEMORY_ACC_BUS == access_channel)
				write_mem_func = aice_usb_write_mem_b_bus;
			else
				write_mem_func = aice_usb_write_mem_b_dim;

			for (i = 0; i < count; i++) {
				write_mem_func (addr, *buffer);
                                buffer++;
                                addr++;
                        }
                        break;
                case 2:
			if (AICE_MEMORY_ACC_BUS == access_channel)
				write_mem_func = aice_usb_write_mem_h_bus;
			else
				write_mem_func = aice_usb_write_mem_h_dim;

			for (i = 0; i < count; i++) {
				uint16_t value;
				memcpy(&value, buffer, sizeof(uint16_t));

				write_mem_func (addr, value);
                                buffer += 2;
                                addr += 2;
                        }
                        break;
                case 4:
			if (AICE_MEMORY_ACC_BUS == access_channel)
				write_mem_func = aice_usb_write_mem_w_bus;
			else
				write_mem_func = aice_usb_write_mem_w_dim;

			for (i = 0; i < count; i++) {
				uint32_t value;
				memcpy(&value, buffer, sizeof(uint32_t));

				write_mem_func (addr, value);
                                buffer += 4;
                                addr += 4;
                        }
                        break;
        }

        return ERROR_OK;
}

static int aice_usb_bulk_read_mem (uint32_t addr, uint32_t count, uint8_t *buffer)
{
	log_add (LOG_DEBUG, "--<aice_usb_bulk_read_mem>, addr: 0x%08x, count: 0x%08x\n", addr, count);

        uint32_t packet_size;

        while (count > 0)
        {
                packet_size = (count >= 0x100) ? 0x100 : count;

		/** set address */
		addr &= 0xFFFFFFFC;
		if (aice_write_misc (current_target_id, NDS_EDM_MISC_SBAR, addr) != ERROR_OK)
			return ERROR_FAIL;

                if (aice_fastread_mem (current_target_id, (uint32_t *)buffer, packet_size) != ERROR_OK)
                        return ERROR_FAIL;

                buffer += (packet_size * 4);
                addr += (packet_size * 4);
                count -= packet_size;
        }

        return ERROR_OK;
}

static int aice_usb_bulk_write_mem (uint32_t addr, uint32_t count, const uint8_t *buffer)
{
	log_add (LOG_DEBUG, "--<aice_usb_bulk_write_mem>, addr: 0x%08x, count: 0x%08x\n", addr, count);

        uint32_t packet_size;

        while (count > 0)
        {
                packet_size = (count >= 0x100) ? 0x100 : count;

		/** set address */
		addr &= 0xFFFFFFFC;
		if (aice_write_misc (current_target_id, NDS_EDM_MISC_SBAR, addr | 1) != ERROR_OK)
			return ERROR_FAIL;

                if (aice_fastwrite_mem (current_target_id, (const uint32_t *)buffer, packet_size) != ERROR_OK)
                        return ERROR_FAIL;

                buffer += (packet_size * 4);
                addr += (packet_size * 4);
                count -= packet_size;
        }

        return ERROR_OK;
}

static int aice_usb_read_tlb (uint32_t virtual_address, uint32_t *physical_address)
{
	log_add (LOG_DEBUG, "--<aice_usb_read_tlb>, virtual address: 0x%08x\n", virtual_address);

	uint32_t instructions[4];
	uint32_t probe_result;
	uint32_t value_mr3;
	uint32_t value_mr4;
	uint32_t access_page_size;
	uint32_t virtual_offset;

	aice_write_dtr (current_target_id, virtual_address);

	/* probe TLB first */
	instructions[0] = MFSR_DTR(R0);
	instructions[1] = TLBOP_TARGET_PROBE(R1, R0);
	instructions[2] = DSB;
	instructions[3] = BEQ_MINUS_12;
	aice_execute_dim (instructions, 4);

	aice_usb_read_reg (R1, &probe_result);

	log_add (LOG_INFO, "\tprobe result: 0x%08x\n", probe_result);

	if (probe_result & 0x80000000)
		return ERROR_FAIL;

	/* read TLB entry */
	aice_write_dtr (current_target_id, probe_result & 0x7FF);

	/* probe TLB first */
	instructions[0] = MFSR_DTR(R0);
	instructions[1] = TLBOP_TARGET_READ(R0);
	instructions[2] = DSB;
	instructions[3] = BEQ_MINUS_12;
	aice_execute_dim (instructions, 4);

	aice_usb_read_reg (MR3, &value_mr3);
	aice_usb_read_reg (MR4, &value_mr4);

	access_page_size = value_mr4 & 0xF;
	if (0 == access_page_size) /* 4K page */
		virtual_offset = virtual_address & 0x00000FFF;
	else if (1 == access_page_size) /* 8K page */
		virtual_offset = virtual_address & 0x00001FFF;
	else if (5 == access_page_size) /* 1M page */
		virtual_offset = virtual_address & 0x000FFFFF;
	else
		return ERROR_FAIL;

	*physical_address = (value_mr3 & 0xFFFFF000) | virtual_offset;

	return ERROR_OK;
}

static int aice_usb_dcache_inval_all (void)
{
	log_add (LOG_DEBUG, "--<aice_usb_dcache_inval_all>\n");

	uint32_t set_index;
	uint32_t way_index;
	uint32_t cache_index;
	uint32_t instructions[4];

	instructions[0] = MFSR_DTR(R0);
	instructions[1] = L1D_IX_INVAL(R0);
	instructions[2] = DSB;
	instructions[3] = BEQ_MINUS_12;

	for (set_index = 0; set_index < dcache.set; set_index++)
	{
		for (way_index = 0; way_index < dcache.way; way_index++)
		{
			cache_index = (way_index << (dcache.log2_set + dcache.log2_line_size)) | 
				(set_index << dcache.log2_line_size);

			if (ERROR_OK != aice_write_dtr (current_target_id, cache_index))
				return ERROR_FAIL;

			if (ERROR_OK != aice_execute_dim (instructions, 4))
				return ERROR_FAIL;
		}
	}

	return ERROR_OK;
}

static int aice_usb_dcache_va_inval (uint32_t address)
{
	log_add (LOG_DEBUG, "--<aice_usb_dcache_va_inval>\n");

	uint32_t instructions[4];

	aice_write_dtr (current_target_id, address);

	instructions[0] = MFSR_DTR(R0);
	instructions[1] = L1D_VA_INVAL(R0);
	instructions[2] = DSB;
	instructions[3] = BEQ_MINUS_12;

	return aice_execute_dim (instructions, 4);
}

static int aice_usb_dcache_wb_all (void)
{
	log_add (LOG_DEBUG, "--<aice_usb_dcache_wb_all>\n");

	uint32_t set_index;
	uint32_t way_index;
	uint32_t cache_index;
	uint32_t instructions[4];

	instructions[0] = MFSR_DTR(R0);
	instructions[1] = L1D_IX_WB(R0);
	instructions[2] = DSB;
	instructions[3] = BEQ_MINUS_12;

	for (set_index = 0; set_index < dcache.set; set_index++)
	{
		for (way_index = 0; way_index < dcache.way; way_index++)
		{
			cache_index = (way_index << (dcache.log2_set + dcache.log2_line_size)) | 
				(set_index << dcache.log2_line_size);

			if (ERROR_OK != aice_write_dtr (current_target_id, cache_index))
				return ERROR_FAIL;

			if (ERROR_OK != aice_execute_dim (instructions, 4))
				return ERROR_FAIL;
		}
	}

	return ERROR_OK;
}

static int aice_usb_dcache_va_wb (uint32_t address)
{
	log_add (LOG_DEBUG, "--<aice_usb_dcache_va_wb>\n");

	uint32_t instructions[4];

	aice_write_dtr (current_target_id, address);

	instructions[0] = MFSR_DTR(R0);
	instructions[1] = L1D_VA_WB(R0);
	instructions[2] = DSB;
	instructions[3] = BEQ_MINUS_12;

	return aice_execute_dim (instructions, 4);
}

static int aice_usb_icache_inval_all (void)
{
	log_add (LOG_DEBUG, "--<aice_usb_icache_inval_all>\n");

	uint32_t set_index;
	uint32_t way_index;
	uint32_t cache_index;
	uint32_t instructions[4];

	instructions[0] = MFSR_DTR(R0);
	instructions[1] = L1I_IX_INVAL(R0);
	instructions[2] = ISB;
	instructions[3] = BEQ_MINUS_12;

	for (set_index = 0; set_index < icache.set; set_index++)
	{
		for (way_index = 0; way_index < icache.way; way_index++)
		{
			cache_index = (way_index << (icache.log2_set + icache.log2_line_size)) | 
				(set_index << icache.log2_line_size);

			if (ERROR_OK != aice_write_dtr (current_target_id, cache_index))
				return ERROR_FAIL;

			if (ERROR_OK != aice_execute_dim (instructions, 4))
				return ERROR_FAIL;
		}
	}

	return ERROR_OK;
}

static int aice_usb_icache_va_inval (uint32_t address)
{
	log_add (LOG_DEBUG, "--<aice_usb_icache_va_inval>\n");

	uint32_t instructions[4];

	aice_write_dtr (current_target_id, address);

	instructions[0] = MFSR_DTR(R0);
	instructions[1] = L1I_VA_INVAL(R0);
	instructions[2] = ISB;
	instructions[3] = BEQ_MINUS_12;

	return aice_execute_dim (instructions, 4);
}

static int aice_usb_init_cache (void)
{
	log_add (LOG_DEBUG, "--<aice_usb_init_cache>\n");

	uint32_t value_cr1;
	uint32_t value_cr2;

	aice_usb_read_reg (CR1, &value_cr1);
	aice_usb_read_reg (CR2, &value_cr2);

	icache.set = value_cr1 & 0x7;
	icache.log2_set = icache.set + 6;
	icache.set = 64 << icache.set;
	icache.way = ((value_cr1 >> 3) & 0x7) + 1;
	icache.line_size = (value_cr1 >> 6) & 0x7;
	if (icache.line_size != 0)
	{
		icache.log2_line_size = icache.line_size + 2;
		icache.line_size = 8 << (icache.line_size - 1);
	}
	else
		icache.log2_line_size = 0;

	log_add (LOG_DEBUG, "\ticache set: %d, way: %d, line size: %d, log2(set): %d, log2(line_size): %d\n",
			icache.set, icache.way, icache.line_size, icache.log2_set, icache.log2_line_size);

	dcache.set = value_cr2 & 0x7;
	dcache.log2_set = dcache.set + 6;
	dcache.set = 64 << dcache.set;
	dcache.way = ((value_cr2 >> 3) & 0x7) + 1;
	dcache.line_size = (value_cr2 >> 6) & 0x7;
	if (dcache.line_size != 0)
	{
		dcache.log2_line_size = dcache.line_size + 2;
		dcache.line_size = 8 << (dcache.line_size - 1);
	}
	else
		dcache.log2_line_size = 0;

	log_add (LOG_DEBUG, "\tdcache set: %d, way: %d, line size: %d, log2(set): %d, log2(line_size): %d\n",
			dcache.set, dcache.way, dcache.line_size, dcache.log2_set, dcache.log2_line_size);

	cache_init = true;

	return ERROR_OK;
}

static int aice_usb_cache_ctl (uint32_t sub_type, uint32_t address)
{
	log_add (LOG_DEBUG, "--<aice_usb_cache_ctl>\n");

	int result;

	if (cache_init == false)
		aice_usb_init_cache ();

	switch (sub_type)
	{
		case AICE_CACHE_CTL_L1D_INVALALL:
			result = aice_usb_dcache_inval_all ();
			break;
		case AICE_CACHE_CTL_L1D_VA_INVAL:
			result = aice_usb_dcache_va_inval (address);
			break;
		case AICE_CACHE_CTL_L1D_WBALL:
			result = aice_usb_dcache_wb_all ();
			break;
		case AICE_CACHE_CTL_L1D_VA_WB:
			result = aice_usb_dcache_va_wb (address);
			break;
		case AICE_CACHE_CTL_L1I_INVALALL:
			result = aice_usb_icache_inval_all ();
			break;
		case AICE_CACHE_CTL_L1I_VA_INVAL:
			result = aice_usb_icache_va_inval (address);
			break;
		default:
			result = ERROR_FAIL;
			break;
	}

	return result;
}

/********************************************************************************************/

static int pipe_read(void *buffer, int length)
{
#ifdef __MINGW32__
	BOOL success;
	DWORD has_read;

	success = ReadFile (GetStdHandle(STD_INPUT_HANDLE), buffer, length, &has_read, NULL);
	if (!success)
	{
		log_add (LOG_DEBUG, "read pipe failed\n");
		return -1;
	}

	return has_read;
#else
	return read (STDIN_FILENO, buffer, length);
#endif
}

static int pipe_write(const void *buffer, int length)
{
#ifdef __MINGW32__
	BOOL success;
	DWORD written;

	success = WriteFile (GetStdHandle(STD_OUTPUT_HANDLE), buffer, length, &written, NULL);
	if (!success)
	{
		log_add (LOG_DEBUG, "write pipe failed\n");
		return -1;
	}

	return written;
#else
	return write (STDOUT_FILENO, buffer, length);
#endif
}

static void aice_open (const char *input)
{
	log_add (LOG_INFO, "<aice_open>: ");

	char response[MAXLINE];
	uint16_t vid;
	uint16_t pid;

	vid = get_u16 (input + 1);
	pid = get_u16 (input + 3);

	log_add (LOG_INFO, "VID: 0x%04x, PID: 0x%04x\n", vid, pid);

	if (ERROR_OK != aice_usb_open(vid, pid)) {
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
	}

	if (ERROR_FAIL == aice_get_version_info()) {
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
	}

	/* attempt to reset Andes EDM */
	if (ERROR_FAIL == aice_edm_reset()) {
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
	}

	if (ERROR_OK != aice_edm_init()) {
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
	}

	response[0] = AICE_OK;
	pipe_write (response, 1);
}

static void aice_set_jtag_clock (const char *input)
{
	log_add (LOG_INFO, "<aice_set_jtag_clock>: ");

	char response[MAXLINE];

	jtag_clock = get_u32 (input + 1);

	log_add (LOG_INFO, "CLOCK: %x\n", jtag_clock);

	if (ERROR_OK != aice_usb_set_clock (jtag_clock)) {
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
	}

	response[0] = AICE_OK;
	pipe_write (response, 1);
}

static void aice_close (const char *input)
{
	log_add (LOG_INFO, "<aice_close>\n");

	char response[MAXLINE];

	aice_usb_close ();

	response[0] = AICE_OK;

	pipe_write (response, 1);

	exit (0);
}

static void aice_reset (const char *input)
{
	log_add (LOG_INFO, "<aice_reset>\n");

	char response[MAXLINE];

	if (ERROR_OK == aice_usb_reset ())
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;

	pipe_write (response, 1);
}

static void aice_assert_srst (const char *input)
{
	log_add (LOG_INFO, "<aice_assert_srst>: ");
	if (input[1] == AICE_SRST)
		log_add (LOG_INFO, "SRST\n");
	else if (input[1] == AICE_RESET_HOLD)
		log_add (LOG_INFO, "RESET HOLD\n");
	else
		log_add (LOG_INFO, "UNKNOWN\n");

	char response[MAXLINE];

	if (ERROR_OK == aice_usb_assert_srst (input[1]))
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;

	pipe_write (response, 1);
}

static void aice_run (const char *input)
{
	log_add (LOG_INFO, "<aice_run>\n");

	char response[MAXLINE];

	if (ERROR_OK == aice_usb_run ())
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;

	pipe_write (response, 1);
}

static void aice_halt (const char *input)
{
	log_add (LOG_INFO, "<aice_halt>\n");

	char response[MAXLINE];

	if (ERROR_OK == aice_usb_halt ())
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;

	pipe_write (response, 1);
}

static void aice_step (const char *input)
{
	log_add (LOG_INFO, "<aice_step>\n");

	char response[MAXLINE];

	if (ERROR_OK == aice_usb_step ())
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;

	pipe_write (response, 1);
}

static void aice_read_reg (const char *input)
{
	log_add (LOG_INFO, "<aice_read_reg>: ");

	char response[MAXLINE];
	uint32_t reg_no = get_u32 (input + 1);
	uint32_t value;

	log_add (LOG_INFO, "reg: %s\n", nds32_reg_simple_name (reg_no));

	if (reg_no == R0)
		value = r0_backup;
	else if (reg_no == R1)
		value = r1_backup;
	else
	{
		if (ERROR_OK != aice_usb_read_reg (reg_no, &value))
			value = 0xBBADBEEF;
	}

	log_add (LOG_INFO, "value: 0x%08x\n", value);

	set_u32 (response, value);
	pipe_write (response, 4);
}

static void aice_write_reg (const char *input)
{
	log_add (LOG_INFO, "<aice_write_reg>: ");

	char response[MAXLINE];
	uint32_t reg_no = get_u32 (input + 1);
	uint32_t value = get_u32 (input + 5);

	log_add (LOG_INFO, "reg: %s, value: 0x%08x\n", nds32_reg_simple_name (reg_no), value);

	response[0] = AICE_OK;

	if (reg_no == R0)
		r0_backup = value;
	else if (reg_no == R1)
		r1_backup = value;
	else
	{
		if (ERROR_OK == aice_usb_write_reg (reg_no, value))
			response[0] = AICE_OK;
		else
			response[0] = AICE_ERROR;
	}

	pipe_write (response, 1);
}

static void aice_read_reg_64 (const char *input)
{
	log_add (LOG_INFO, "<aice_read_reg_64>: ");

	char response[MAXLINE];
	uint32_t reg_no = get_u32 (input + 1);
	uint32_t value;
	uint32_t high_value;

	log_add (LOG_INFO, "reg: %s\n", nds32_reg_simple_name (reg_no));

	if (ERROR_OK != aice_usb_read_reg (reg_no, &value))
		value = 0xBBADBEEF;
	aice_usb_read_reg (R1, &high_value);

	log_add (LOG_INFO, "low: 0x%08x, high: 0x%08x\n", value, high_value);

	set_u32 (response, value);
	set_u32 (response + 4, high_value);

	pipe_write (response, 8);
}

static void aice_write_reg_64 (const char *input)
{
	log_add (LOG_INFO, "<aice_write_reg_64>: ");

	char response[MAXLINE];
	uint32_t reg_no = get_u32 (input + 1);
	uint32_t value = get_u32 (input + 5);
	uint32_t high_value = get_u32 (input + 9);

	log_add (LOG_INFO, "reg: %s, low: 0x%08x, high: 0x%08x\n", nds32_reg_simple_name (reg_no), value, high_value);

	response[0] = AICE_OK;

	aice_usb_write_reg (R1, high_value);
	if (ERROR_OK == aice_usb_write_reg (reg_no, value))
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;

	pipe_write (response, 1);
}

static void aice_read_mem_unit (const char *input)
{
	log_add (LOG_INFO, "<aice_read_mem_unit>: ");

	char response[MAXLINE];
	uint32_t addr = get_u32 (input + 1);
	uint32_t size = get_u32 (input + 5);
	uint32_t count = get_u32 (input + 9);

	log_add (LOG_INFO, "addr: 0x%08x, size: %d, count: %d\n", addr, size, count);

	if (AICE_MEMORY_ACC_CPU == access_channel)
	{
		aice_usb_set_address_dim(addr);
	}

	aice_usb_read_memory_unit (addr, size, count, (uint8_t *)response);

	pipe_write (response, size * count);
}

static void aice_write_mem_unit (const char *input)
{
	log_add (LOG_INFO, "<aice_write_mem_unit>: ");

	char response[MAXLINE];
	uint32_t addr = get_u32 (input + 1);
	uint32_t size = get_u32 (input + 5);
	uint32_t count = get_u32 (input + 9);

	log_add (LOG_INFO, "addr: 0x%08x, size: %d, count: %d\n", addr, size, count);

	if (AICE_MEMORY_ACC_CPU == access_channel)
	{
		aice_usb_set_address_dim(addr);
	}

	if (ERROR_OK == aice_usb_write_memory_unit (addr, size, count, (uint8_t *)(input + 13)))
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;

	pipe_write (response, 1);
}

static void aice_read_mem_bulk (const char *input)
{
	log_add (LOG_INFO, "<aice_read_mem_bulk>: ");

	char response[MAXLINE + 1];
	uint32_t addr = get_u32 (input + 1);
	uint32_t length = get_u32 (input + 5);
	int retval;

	log_add (LOG_INFO, "addr: 0x%08x, length: 0x%08x\n", addr, length);

	if (AICE_MEMORY_ACC_CPU == access_channel)
	{
		aice_usb_set_address_dim(addr);
	}

	if (AICE_MEMORY_ACC_CPU == access_channel)
		retval = aice_usb_read_memory_unit(addr, 4, length / 4, (uint8_t *)(response + 1));
	else
		retval = aice_usb_bulk_read_mem(addr, length / 4, (uint8_t *)(response + 1));

	if (retval != ERROR_OK)
	{
		response[0] = AICE_ERROR;
		pipe_write (response, 1);
		return;
	}

	response[0] = AICE_OK;
	pipe_write (response, length + 1);
}

static void aice_write_mem_bulk (const char *input)
{
	log_add (LOG_INFO, "<aice_write_mem_bulk>: ");

	char response[MAXLINE];
	char remain_data[MAXLINE];
	uint32_t addr = get_u32 (input + 1);
	uint32_t length = get_u32 (input + 5);
	int write_len;
	int expected_len;
	char expected_len_buf[4];
	uint32_t remain_len = length;
	uint32_t write_addr = addr;
	int retval;

	log_add (LOG_INFO, "addr: 0x%08x, length: 0x%08x\n", addr, length);

	response[0] = AICE_ACK;
	pipe_write (response, 1);

	if (AICE_MEMORY_ACC_CPU == access_channel)
	{
		aice_usb_set_address_dim(addr);
	}

	while (remain_len > 0)
	{
		uint32_t n;
		if ((n = pipe_read (expected_len_buf, 4)) < 0)
		{
			response[0] = AICE_ERROR;
			pipe_write (response, 1);
			return;
		}
		log_add (LOG_DEBUG, "\tn: %x\n", n);

		expected_len = get_u32 (expected_len_buf);

		// debug
		log_add (LOG_DEBUG, "\tread 4 bytes first, %x\n", expected_len);

		write_len = 0;
		while (write_len != expected_len)
		{
			write_len += pipe_read (remain_data + write_len, MAXLINE);

			// debug
			log_add (LOG_DEBUG, "\twrite_len: %x\n", write_len);
		}
		log_add (LOG_DEBUG, "\tremain_len: 0x%08x, write_len: 0x%08x\n", remain_len, write_len);

		if (AICE_MEMORY_ACC_CPU == access_channel)
			retval = aice_usb_write_memory_unit (write_addr, 4, write_len / 4, (uint8_t *)remain_data);
		else
			retval = aice_usb_bulk_write_mem(write_addr, write_len / 4, (uint8_t *)remain_data);

		if (retval != ERROR_OK)
		{
			response[0] = AICE_ERROR;
			pipe_write (response, 1);
			return;
		}

		remain_len -= write_len;
		write_addr += write_len;

		/* write_mem_bulk complete */
		if (remain_len == 0)
			response[0] = AICE_OK;
		else
			response[0] = AICE_ACK;

		pipe_write (response, 1);
	}
}

static void aice_read_debug_reg (const char *input)
{
	log_add (LOG_INFO, "<aice_read_debug_reg>: ");

	char response[MAXLINE];
	uint32_t addr = get_u32 (input + 1);
	uint32_t value;

	log_add (LOG_INFO, "addr: 0x%08x\n", addr);

	if (ERROR_OK == aice_usb_read_debug_reg (addr, &value))
		set_u32 (response, value);
	else
		set_u32 (response, 0xBBADBEEF);

	log_add (LOG_INFO, "value: 0x%08x\n", value);

	pipe_write (response, 4);
}

static void aice_write_debug_reg (const char *input)
{
	log_add (LOG_INFO, "<aice_write_debug_reg>: ");

	char response[MAXLINE];
	uint32_t addr = get_u32 (input + 1);
	uint32_t value = get_u32 (input + 5);

	log_add (LOG_INFO, "addr: 0x%08x, value: 0x%08x\n", addr, value);

	if (ERROR_OK == aice_usb_write_debug_reg (addr, value))
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;

	pipe_write (response, 1);
}

static void aice_idcode (const char *input)
{
	log_add (LOG_INFO, "<aice_idcode>:\n");

	char response[MAXLINE];
	uint8_t num_of_idcode;
	uint32_t idcode[MAX_ID_CODE];
	uint8_t i;

	if (ERROR_OK == aice_scan_chain(idcode, &num_of_idcode))
	{
		if (num_of_idcode > MAX_ID_CODE)
		{
			response[0] = 0;
			pipe_write (response, 1);
			return;
		}

		response[0] = num_of_idcode;
		for (i = 0 ; i < num_of_idcode ; i++)
		{
			set_u32 (response + 1 + i*4, idcode[i]);
		}
	}
	else
	{
		num_of_idcode = 0;
		response[0] = 0;
	}

	log_add (LOG_INFO, "# of target: %d, IDCode0: 0x%08x\n", num_of_idcode, idcode[0]);

	pipe_write (response, 1 + num_of_idcode * 4);
}

static void aice_state (const char *input)
{
	log_add (LOG_INFO, "<aice_state>:\n");

	char response[MAXLINE];
	enum aice_target_state_s state = aice_usb_state ();

	response[0] = state;

	pipe_write (response, 1);
}

#if 0
static void aice_select_target (const char *input)
{
	log_add (LOG_INFO, "<aice_select_target>: ");

	char response[MAXLINE];

	current_target_id = get_u32 (input + 1);

	log_add (LOG_INFO, "TARGET ID: %x\n", current_target_id);

	response[0] = AICE_OK;
	pipe_write (response, 1);
}
#endif

static void aice_memory_access (const char *input)
{
	log_add (LOG_INFO, "<aice_memory_access>: ");

	char response[MAXLINE];

	access_channel = get_u32 (input + 1);

	log_add (LOG_INFO, "access channel: %s\n", AICE_MEMORY_ACCESS_NAME[access_channel]);

	response[0] = AICE_OK;
	pipe_write (response, 1);
}

static void aice_memory_mode (const char *input)
{
	log_add (LOG_INFO, "<aice_memory_mode>: ");

	char response[MAXLINE];

	memory_mode = get_u32 (input + 1);

	if (AICE_MEMORY_MODE_AUTO != memory_mode)
	{
		aice_write_misc (current_target_id, NDS_EDM_MISC_ACC_CTL, memory_mode - 1);
		memory_mode_auto_select = false;
	}
	else
	{
		aice_write_misc (current_target_id, NDS_EDM_MISC_ACC_CTL, AICE_MEMORY_MODE_MEM - 1);
		memory_mode_auto_select = true;
	}

	log_add (LOG_INFO, "memory mode: %s\n", AICE_MEMORY_MODE_NAME[memory_mode]);

	response[0] = AICE_OK;
	pipe_write (response, 1);
}

static void aice_read_tlb (const char *input)
{
	log_add (LOG_INFO, "<aice_read_tlb>: ");

	char response[MAXLINE];
	uint32_t virtual_address = get_u32 (input + 1);
	uint32_t physical_address;

	log_add (LOG_INFO, "virtual address: 0x%08x\n", virtual_address);

	if (ERROR_OK == aice_usb_read_tlb (virtual_address, &physical_address))
	{
		response[0] = AICE_OK;
		set_u32 (response + 1, physical_address);

		log_add (LOG_INFO, "converted physical address: 0x%08x\n", physical_address);
	}
	else
	{
		response[0] = AICE_ERROR;

		log_add (LOG_INFO, "TLB miss\n");
	}

	pipe_write (response, 5);
}

static void aice_cache_ctl (const char *input)
{
	log_add (LOG_INFO, "<aice_cache_ctl>: ");

	char response[MAXLINE];
	uint32_t sub_type = get_u32 (input + 1);
	uint32_t address = get_u32 (input + 5);

	log_add (LOG_INFO, "sub type: 0x%08x, address: 0x%08x\n", sub_type, address);

	if (ERROR_OK == aice_usb_cache_ctl (sub_type, address))
		response[0] = AICE_OK;
	else
		response[0] = AICE_ERROR;

	pipe_write (response, 1);
}

int main ()
{
	char line[MAXLINE];
	int n;

	signal(SIGINT, SIG_IGN);
	atexit (log_finalize);

	log_init (512*1024, LOG_DEBUG, true);

	nds32_reg_init ();

	while ((n = pipe_read (line, MAXLINE)) > 0)
	{
		switch (line[0])
		{
			case AICE_OPEN:
				aice_open (line);
				break;
			case AICE_CLOSE:
				aice_close (line);
				break;
			case AICE_RESET:
				aice_reset (line);
				break;
			case AICE_ASSERT_SRST:
				aice_assert_srst (line);
				break;
			case AICE_RUN:
				aice_run (line);
				break;
			case AICE_HALT:
				aice_halt (line);
				break;
			case AICE_STEP:
				aice_step (line);
				break;
			case AICE_READ_REG:
				aice_read_reg (line);
				break;
			case AICE_WRITE_REG:
				aice_write_reg (line);
				break;
			case AICE_READ_REG_64:
				aice_read_reg_64 (line);
				break;
			case AICE_WRITE_REG_64:
				aice_write_reg_64 (line);
				break;
			case AICE_READ_MEM_UNIT:
				aice_read_mem_unit (line);
				break;
			case AICE_WRITE_MEM_UNIT:
				aice_write_mem_unit (line);
				break;
			case AICE_READ_MEM_BULK:
				aice_read_mem_bulk (line);
				break;
			case AICE_WRITE_MEM_BULK:
				aice_write_mem_bulk (line);
				break;
			case AICE_READ_DEBUG_REG:
				aice_read_debug_reg (line);
				break;
			case AICE_WRITE_DEBUG_REG:
				aice_write_debug_reg (line);
				break;
			case AICE_IDCODE:
				aice_idcode (line);
				break;
			case AICE_STATE:
				aice_state (line);
				break;
			case AICE_SET_JTAG_CLOCK:
				aice_set_jtag_clock (line);
				break;
			case AICE_MEMORY_ACCESS:
				aice_memory_access (line);
				break;
			case AICE_MEMORY_MODE:
				aice_memory_mode (line);
				break;
			case AICE_READ_TLB:
				aice_read_tlb (line);
				break;
			case AICE_CACHE_CTL:
				aice_cache_ctl (line);
				break;
		}
	}

	return 0;
}
