source [find dmi.tcl]

set NDS_TAP "123"
scan [jtag names] "%s" NDS_TAP

set DMI_DMCONTROL      0x10
set DMI_DMCONTROL_HALTREQ   0x80000000

proc set_srst {delay_ms} {
	ftdi_write_pin nSRST 0
	sleep $delay_ms
}

proc clear_srst {delay_ms} {
	ftdi_write_pin nSRST 1
	sleep $delay_ms
}

proc set_dbgi {delay_ms} {
	global NDS_TAP
	global DMI_DMCONTROL

	set test_dmcontrol [dmi_read $NDS_TAP $DMI_DMCONTROL]
	set test_dmcontrol [expr $test_dmcontrol | $DMI_DMCONTROL_HALTREQ]
	dmi_write $NDS_TAP $DMI_DMCONTROL $test_dmcontrol
	sleep $delay_ms
}

proc clear_dbgi {delay_ms} {
	global NDS_TAP
	global DMI_DMCONTROL

	set test_dmcontrol [dmi_read $NDS_TAP $DMI_DMCONTROL]
	set test_dmcontrol [expr $test_dmcontrol & ~$DMI_DMCONTROL_HALTREQ]
	dmi_write $NDS_TAP $DMI_DMCONTROL $test_dmcontrol
	sleep $delay_ms
}

proc set_trst {delay_ms} {
	ftdi_write_pin nTRST 0
	sleep $delay_ms
}

proc clear_trst {delay_ms} {
	ftdi_write_pin nTRST 1
	sleep $delay_ms
}

proc write_pins {pattern} {
	puts $pattern
	set pattern_len [string length $pattern]

	for {set i 0} {$i < $pattern_len} {incr i} {
		scan [string index $pattern $i] %d nibble

		set tms [expr [expr $nibble & 0x2] >> 1]
		set tdi [expr [expr $nibble & 0x4] >> 2]
		#puts [format "tms: %d, tdi: %d" $tms $tdi]
		ftdi_clock_tms_out $tms $tdi
	}

}

proc delay {delay_ms} {
	sleep $delay_ms
}

