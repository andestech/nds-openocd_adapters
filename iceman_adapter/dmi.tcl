
proc dmi_read {tap addr} {
	set DTM_IR_DMI 0x11
	irscan $tap $DTM_IR_DMI

	scan [drscan $tap 2 0x1 32 0x0 7 $addr] "%x %x %x" op rdata addr_out
	runtest 5
	scan [drscan $tap 2 0x0 32 0x0 7 $addr] "%x %x %x" op rdata addr_out

	return $rdata
}

proc dmi_write {tap addr wdata} {
	set DTM_IR_DMI 0x11
	irscan $tap $DTM_IR_DMI

	scan [drscan $tap 2 0x2 32 $wdata 7 $addr] "%x %x %x" op rdata addr_out
	runtest 5
	scan [drscan $tap 2 0x1 32 $wdata 7 $addr] "%x %x %x" op rdata addr_out
}

proc reg_read_abstract {tap reg_num} {
	set GDB_REGNO_XPR0     0
	set GDB_REGNO_XPR31    31
	set GDB_REGNO_PC       32
	set GDB_REGNO_FPR0     33
	set GDB_REGNO_FPR31    64
	set GDB_REGNO_CSR0     65
	set GDB_REGNO_CSR4095  4160
	set regdata 0

	if [ expr $reg_num <= $GDB_REGNO_XPR31 ] {
		#puts [format "XPR_number = 0x%x" $reg_num]
		set reg_num [expr $reg_num - $GDB_REGNO_XPR0]
		set reg_num [expr $reg_num + 0x1000]
	} elseif [ expr $reg_num <= $GDB_REGNO_FPR31 ] {
		#puts [format "FPR_number = 0x%x" $reg_num]
		set reg_num [expr $reg_num - $GDB_REGNO_FPR0]
		set reg_num [expr $reg_num + 0x1020]
	} elseif [ expr $reg_num <= $GDB_REGNO_CSR4095 ] {
		#puts [format "CSR_number = 0x%x" $reg_num]
		set reg_num [expr $reg_num - $GDB_REGNO_CSR0]
	} else {
		puts [format "err_reg_num = 0x%x" $reg_num]
		nds script_status 1
		return $regdata
	}
	#puts [format "reg_num = 0x%x" $reg_num]
	set AC_ACCESS_REGISTER_WRITE        0x0010000
	set AC_ACCESS_REGISTER_TRANSFER     0x0020000
	set AC_ACCESS_REGISTER_POSTEXEC     0x0040000
	set AC_ACCESS_REGISTER_SIZE_32      0x0200000
	set AC_ACCESS_REGISTER_SIZE_64      0x0300000

	scan [nds target_xlen] "%x" NDS_TARGET_XLEN
	#puts [format "target_xlen = 0x%x" $NDS_TARGET_XLEN]

	set DMI_DATA0                0x04
	set DMI_ABSTRACTCS           0x16
	set DMI_COMMAND              0x17
	set DMI_ABSTRACTCS_CMDERR    0x0700

	if [ expr $NDS_TARGET_XLEN == 32 ] {
		set command	    $AC_ACCESS_REGISTER_SIZE_32
	} else {
		set command	    $AC_ACCESS_REGISTER_SIZE_64
	}

	set command [expr $command | $reg_num | $AC_ACCESS_REGISTER_TRANSFER]
	dmi_write $tap $DMI_COMMAND $command
	set dmi_stat [dmi_read $tap $DMI_ABSTRACTCS]
	set dmi_error [expr $dmi_stat & $DMI_ABSTRACTCS_CMDERR]

	set regdata [dmi_read $tap $DMI_DATA0]
	if [ expr $NDS_TARGET_XLEN == 64 ] {
		set reg_value_h [dmi_read $tap [expr $DMI_DATA0 + 1]]
		set reg_value_h [expr $reg_value_h << 32]
		set regdata [expr $regdata | $reg_value_h]
	}
	return $regdata
}

proc reg_write_abstract {tap reg_num reg_value} {
	set GDB_REGNO_XPR0     0
	set GDB_REGNO_XPR31    31
	set GDB_REGNO_PC       32
	set GDB_REGNO_FPR0     33
	set GDB_REGNO_FPR31    64
	set GDB_REGNO_CSR0     65
	set GDB_REGNO_CSR4095  4160
	set regdata 0

	if [ expr $reg_num <= $GDB_REGNO_XPR31 ] {
		#puts [format "XPR_number = 0x%x" $reg_num]
		set reg_num [expr $reg_num - $GDB_REGNO_XPR0]
		set reg_num [expr $reg_num + 0x1000]
	} elseif [ expr $reg_num <= $GDB_REGNO_FPR31 ] {
		#puts [format "FPR_number = 0x%x" $reg_num]
		set reg_num [expr $reg_num - $GDB_REGNO_FPR0]
		set reg_num [expr $reg_num + 0x1020]
	} elseif [ expr $reg_num <= $GDB_REGNO_CSR4095 ] {
		#puts [format "CSR_number = 0x%x" $reg_num]
		set reg_num [expr $reg_num - $GDB_REGNO_CSR0]
	} else {
		puts [format "err_reg_num = 0x%x" $reg_num]
		nds script_status 1
		return $regdata
	}
	#puts [format "reg_num = 0x%x" $reg_num]
	set AC_ACCESS_REGISTER_WRITE        0x0010000
	set AC_ACCESS_REGISTER_TRANSFER     0x0020000
	set AC_ACCESS_REGISTER_POSTEXEC     0x0040000
	set AC_ACCESS_REGISTER_SIZE_32      0x0200000
	set AC_ACCESS_REGISTER_SIZE_64      0x0300000

	scan [nds target_xlen] "%x" NDS_TARGET_XLEN
	#puts [format "target_xlen = 0x%x" $NDS_TARGET_XLEN]

	set DMI_DATA0                0x04
	set DMI_DATA1                0x05
	set DMI_ABSTRACTCS           0x16
	set DMI_COMMAND              0x17
	set DMI_ABSTRACTCS_CMDERR    0x0700

	if [ expr $NDS_TARGET_XLEN == 32 ] {
		dmi_write $tap $DMI_DATA0 $reg_value
		set command	    $AC_ACCESS_REGISTER_SIZE_32
	} else {
		set reg_value_h [expr $reg_value >> 32]
		set reg_value [expr $reg_value & 0xFFFFFFFF]
		dmi_write $tap $DMI_DATA0 $reg_value
		dmi_write $tap $DMI_DATA1 $reg_value_h
		set command	    $AC_ACCESS_REGISTER_SIZE_64
	}

	set command [expr $command | $reg_num | $AC_ACCESS_REGISTER_TRANSFER | $AC_ACCESS_REGISTER_WRITE]
	dmi_write $tap $DMI_COMMAND $command
	set dmi_stat [dmi_read $tap $DMI_ABSTRACTCS]
	set dmi_error [expr $dmi_stat & $DMI_ABSTRACTCS_CMDERR]

	return $regdata
}



