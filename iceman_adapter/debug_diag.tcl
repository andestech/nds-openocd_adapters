
# Main
source [find openocd.cfg]
source [find debug_util.tcl]
set verbosity 100
set time_target_sec 1
#adapter_khz 10000
#source [find interface/olimex-arm-usb-tiny-h.cfg]


# Define
set CSR_MVENDORID  0xf11
set CSR_MARCHID    0xf12
set CSR_MIMPID     0xf13
set CSR_MISA       0x301
set CSR_DCSR       0x7b0
set CSR_MNVEC      0x7c3


set test_dm_operate_pass "NG"
set test_dtm_connect_pass "NG"
set test_memory_access_pass "NG"
set test_reset_and_debug_pass "NG"
set test_frequency_pass "NG"


proc test_memory_rw {tap start_addr} {
	set count 4
	set wdata_list {0xaaaaaaaa 0xbbbbbbbb 0xcccccccc 0xdddddddd}

	puts [format "Testing memory write addr = 0x%x" $start_addr]
	for {set i 0} {$i < $count} {incr i} {
		set addr [expr $start_addr + $i*4]
		set wdata [lindex $wdata_list $i]
		write_memory_word $tap $addr $wdata
	}

	puts [format "Testing memory read addr = 0x%x" $start_addr]
	for {set i 0} {$i < $count} {incr i} {
		set addr [expr $start_addr + $i*4]
		set wdata [lindex $wdata_list $i]

		set rdata [read_memory_word $tap $addr]
		#assert {$rdata == $wdata } [format "read/write memory mismatch: addr=0x%x, wdata=0x%x, rdata=0x%x" $addr $wdata $rdata]
		if [ expr $rdata != $wdata ] {
			puts [format "read/write memory mismatch: addr=0x%x, wdata=0x%x, rdata=0x%x" $addr $wdata $rdata]
			return
		}
	}
	global test_memory_access_pass
	set test_memory_access_pass "PASS"
}

proc test_reset_and_halt_all_harts {tap} {
	puts "Testing reset_and_halt_all_harts"
	reset_and_halt_all_harts $tap
	# wait 1 sconds
	after 1000
	global CSR_MNVEC
	global test_reset_and_debug_pass
	global scan_hart_nums
	for {set hartsel 0} {$hartsel < $scan_hart_nums} {incr $hartsel} {
		select_single_hart $tap $hartsel
		set dmstatus [read_dmi_dmstatus $tap]
		set dmstatus_anyunavail [expr ($dmstatus>>12)&0x1]
		if {$dmstatus_anyunavail} {
				puts [format "core%d is unavailable" $hartsel]
				break;
		}

		#set hartxlen 32
		targets $tap$hartsel
		scan [nds target_xlen] "%x" hartxlen

		set pc [read_dpc $tap $hartxlen]
		puts [format "Hart %d pc = 0x%x" $hartsel $pc]
		set mnvec [read_register $tap $hartxlen $CSR_MNVEC]
		if [ expr $pc != $mnvec ] {
			puts [format "pc != mnvec"]
			return
		}
	}
	set test_reset_and_debug_pass "PASS"
}

proc test_frequency {tap} {
	set nds_if_pass "PASS"
	# Get current speed
	scan [nds adapter_khz] "%x" cur_adapter_khz

	# Change to current speed/2
	set test_adapter_khz [expr $cur_adapter_khz >> 1]
	puts [format "Setting adapter speed: %d kHz" $test_adapter_khz]
	nds adapter_khz $test_adapter_khz

	# Get speed again
	scan [nds adapter_khz] "%x" test2_adapter_khz
	puts [format "Current adapter speed: %d kHz" $test2_adapter_khz]

	if [ expr $test2_adapter_khz == $test_adapter_khz ] {
		puts [format "Success to set speed!!"]
	} else {
		puts [format "Unable to set speed!!"]
		set nds_if_pass "NG"
	}
	# Restore cur_speed
	nds adapter_khz $cur_adapter_khz

	return $nds_if_pass
}

proc get_march_name {marchid} {
	set arch_name_list {"N" "D" "E" "S" "" "" "" "" "NX" "DX" "EX" "SX"}
	set name_idx [expr ($marchid>>12)&0xf]
	set march_name [lindex $arch_name_list $name_idx]
	set march_name_out [format "%s%d%d" $march_name [expr ($marchid>>4)&0xf] [expr ($marchid>>0)&0xf] ]
	#puts [format "march_name_1 %s" $march_name_out]
	return $march_name_out
}

proc get_platform_name {platformid} {
	set platform_name_out [format "%c%c%d%d%d" [expr ($platformid>>24)&0xff] [expr ($platformid>>16)&0xff] [expr ($platformid>>12)&0xf] [expr ($platformid>>8)&0xf] [expr ($platformid>>4)&0xf]]
	return $platform_name_out
}


set time_start [clock seconds]
set time_end   [clock seconds]
while {[expr $time_end-$time_start] < $time_target_sec} {
	set test_dm_operate_pass "NG"
	set test_dtm_connect_pass "NG"
	set test_memory_access_pass "NG"
	set test_reset_and_debug_pass "NG"
	set test_frequency_pass "NG"

	#jtag newtap target dtm -irlen 5 -expected-id 0x1000563D
	scan [jtag names] "%s" NDS_TAP
	puts [format "get jtag name = %s" $NDS_TAP]
	#set target.dtm $NDS_TAP

	init

	puts "Initializing Debug Module Interface"
	init_dmi $NDS_TAP

	puts "Initializing Debug Module"
	reset_dm $NDS_TAP
	reset_ndm $NDS_TAP
	set test_dm_operate_pass "PASS"

	puts "Scanning number of harts"
	set scan_hart_nums [scan_harts $NDS_TAP]
	puts [format "scan_hart_nums=0x%x" $scan_hart_nums]
	if [ expr $scan_hart_nums > 0 ] {
		set test_dtm_connect_pass "PASS"
	} else {
		exit
	}
	set test_frequency_pass [test_frequency $NDS_TAP]

	for {set hartsel 0} {$hartsel < $scan_hart_nums} {incr $hartsel} {
		select_single_hart $NDS_TAP $hartsel
		set dmstatus [read_dmi_dmstatus $NDS_TAP]
		set dmstatus_anyunavail [expr ($dmstatus>>12)&0x1]
		if {$dmstatus_anyunavail} {
				puts [format "core%d is unavailable" $hartsel]
				break;
		}
		puts [format "Halting Hart %d" $hartsel]
		halt_hart $NDS_TAP $hartsel

		targets $NDS_TAP$hartsel
		scan [nds target_xlen] "%x" hartxlen
		puts [format "core%d: target_xlen = 0x%x" $hartsel $hartxlen]

		set mvendorid [read_register $NDS_TAP $hartxlen $CSR_MVENDORID]
		puts [format "core%d: mvendorid=0x%x" $hartsel $mvendorid]
		set marchid [read_register $NDS_TAP $hartxlen $CSR_MARCHID]
		set march_name [get_march_name $marchid]
		puts [format "core%d: marchid=0x%x %s" $hartsel $marchid $march_name]

		set mimpid [read_register $NDS_TAP $hartxlen $CSR_MIMPID]
		puts [format "core%d: mimpid=0x%x" $hartsel $mimpid]
		set misa [read_register $NDS_TAP $hartxlen $CSR_MISA]
		puts [format "core%d: misa=0x%x" $hartsel $misa]
		set dcsr [read_register $NDS_TAP $hartxlen $CSR_DCSR]
		puts [format "core%d: dcsr=0x%x" $hartsel $dcsr]
		set mnvec [read_register $NDS_TAP $hartxlen $CSR_MNVEC]
		puts [format "core%d: mnvec=0x%x" $hartsel $mnvec]
		set pc [read_dpc $NDS_TAP $hartxlen]
		puts [format "core%d: pc = 0x%x" $hartsel $pc]

		set regaddr 0xF0100000
		set rdata [read_memory_word $NDS_TAP $regaddr]
		set platform_name [get_platform_name $rdata]
		puts [format "REG_SMU=0x%x %s" $rdata $platform_name]

		test_memory_rw $NDS_TAP $NDS_MEM_ADDR
	}

	test_reset_and_halt_all_harts $NDS_TAP

	puts [format "********************"]
	puts [format "Diagnostic Report"]
	puts [format "********************"]

	puts [format "(%s) check changing the JTAG frequency ..." $test_frequency_pass]
	puts [format "(%s) check JTAG/DTM connectivity ..." $test_dtm_connect_pass]
	puts [format "(%s) check that Debug Module (DM) is operational ..." $test_dm_operate_pass]
	puts [format "(%s) check reset-and-debug ..." $test_reset_and_debug_pass]
	#puts [format "(%s) check that Program buffer and CPU domain are operational ..." $test_pbuf_work_pass]
	puts [format "(%s) check accessing memory through CPU ..." $test_memory_access_pass]
	puts [format "********************"]

	set time_end [clock seconds]
}

exit
