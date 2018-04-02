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

proc test_reset_and_debug {tap} {
	set nds_if_pass "PASS"
	
	puts [format "ASSERTING NDRESET"]
	set DMI_DMCONTROL      0x10
	set DMI_DMSTATUS       0x11
	set DMI_DMCONTROL_NDMRESET  0x02
	set DMI_DMCONTROL_HALTREQ   0x80000000
	set test_dmcontrol [dmi_read $tap $DMI_DMCONTROL]

	set test_dmcontrol [expr $test_dmcontrol | $DMI_DMCONTROL_NDMRESET]
	set test_dmcontrol [expr $test_dmcontrol | $DMI_DMCONTROL_HALTREQ]
	dmi_write $tap $DMI_DMCONTROL $test_dmcontrol

	# alive_sleep(nds32->reset_time);
	sleep 3000

	# deassert reset
	set test_dmcontrol [dmi_read $tap $DMI_DMCONTROL]
	#puts [format "test_dmcontrol 0x%x" $test_dmcontrol]
	
	# Clear the reset, but make sure haltreq is still set
	set test_dmcontrol [expr $test_dmcontrol & ~$DMI_DMCONTROL_NDMRESET]
	set test_dmcontrol [expr $test_dmcontrol | $DMI_DMCONTROL_HALTREQ]
	#puts [format "test_dmcontrol 0x%x" $test_dmcontrol]
	dmi_write $tap $DMI_DMCONTROL $test_dmcontrol
	
	#puts [format "DEASSERTING RESET, waiting for hart to be halted"]
	
	set DMI_DMSTATUS_ALLHALTED   0x200
	set test_dmstatus [dmi_read $tap $DMI_DMSTATUS]
	#puts [format "test_dmstatus 0x%x" $test_dmstatus]
	while { [expr $test_dmstatus & $DMI_DMSTATUS_ALLHALTED] == 0x00 } {
		sleep 1 
		set retry [expr $retry + 1]
		if [ expr $retry >= 100 ] {
			puts [format "DMSTATUS ALLHALTED Failed!!"]
			set nds_if_pass "NG"
			return $nds_if_pass
		}
		set test_dmstatus [dmi_read $tap $DMI_DMSTATUS]
	}

	# clear halt
	set test_dmcontrol [expr $test_dmcontrol & ~$DMI_DMCONTROL_HALTREQ]
	dmi_write $tap $DMI_DMCONTROL $test_dmcontrol
	return $nds_if_pass
}
