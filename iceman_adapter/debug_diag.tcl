set test_dm_operate_pass "NG"
set test_dtm_connect_pass "NG"
set test_memory_access_pass "NG"
set test_reset_and_debug_pass "NG"
set test_frequency_pass "NG"

set CSR_MVENDORID  0xf11
set CSR_MARCHID    0xf12
set CSR_MIMPID     0xf13
set CSR_MISA       0x301
set CSR_DCSR       0x7b0


# main
source [find openocd.cfg]
#adapter_khz 10000
#source [find interface/olimex-arm-usb-tiny-h.cfg]

set verbosity 100
source [find debug_util.tcl]

proc test_memory_rw {tap start_addr} {
	set count 4
	set wdata_list {0xaaaaaaaa 0xbbbbbbbb 0xcccccccc 0xdddddddd}

	print_info [format "Testing memory write addr = 0x%x" $start_addr]
	for {set i 0} {$i < $count} {incr i} {
		set addr [expr $start_addr + $i*4]
		set wdata [lindex $wdata_list $i]
		print_info [format "*0x%x = 0x%x" $addr $wdata]
		write_memory_word $tap $addr $wdata 
	}

	print_info [format "Testing memory read addr = 0x%x" $start_addr]
	for {set i 0} {$i < $count} {incr i} {
		set addr [expr $start_addr + $i*4]
		set wdata [lindex $wdata_list $i]
		
		set rdata [read_memory_word $tap $addr]
		print_info [format "*0x%x = 0x%x" $addr $rdata]
		#assert {$rdata == $wdata } [format "read/write memory mismatch: addr=0x%x, wdata=0x%x, rdata=0x%x" $addr $wdata $rdata]
		if [ expr $rdata != $wdata ] {
			print_info [format "read/write memory mismatch: addr=0x%x, wdata=0x%x, rdata=0x%x" $addr $wdata $rdata]
			return
		}
	}
	set test_memory_access_pass "PASS"
}

proc test_reset_and_halt_all_harts {tap} {
	print_info "Testing reset_and_halt_all_harts"
	reset_and_halt_all_harts $tap
	# wait 1 sconds
	after 1000
	scan_harts $tap

	set hartsel 0
	set hartxlen 32

	select_single_hart $tap $hartsel
	set pc [read_dpc $tap $hartxlen]

	print_info [format "Hart %d pc = 0x%x" $hartsel $pc]
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

#jtag newtap target dtm -irlen 5 -expected-id 0x1000563D
scan [jtag names] "%s" NDS_TAP
puts [format "get jtag name = %s" $NDS_TAP]
#set target.dtm $NDS_TAP

init

print_debug_msg "Initializing Debug Module Interface"
init_dmi $NDS_TAP

print_debug_msg "Initializing Debug Module"
reset_dm $NDS_TAP
reset_ndm $NDS_TAP
set test_dm_operate_pass "PASS"

print_debug_msg "Scanning number of harts"
scan_harts $NDS_TAP
set test_dtm_connect_pass "PASS"

print_info "Halting Hart 0"
set hartsel 0
set hartxlen 32
halt_hart $NDS_TAP $hartsel

set mvendorid [read_register $NDS_TAP $hartxlen $CSR_MVENDORID]
print_info [format "mvendorid=0x%x" $mvendorid]
set marchid [read_register $NDS_TAP $hartxlen $CSR_MARCHID]
print_info [format "marchid=0x%x" $marchid]
set mimpid [read_register $NDS_TAP $hartxlen $CSR_MIMPID]
print_info [format "mimpid=0x%x" $mimpid]
set misa [read_register $NDS_TAP $hartxlen $CSR_MISA]
print_info [format "misa=0x%x" $misa]
set dcsr [read_register $NDS_TAP $hartxlen $CSR_DCSR]
print_info [format "dcsr=0x%x" $dcsr]

test_memory_rw $NDS_TAP $NDS_MEM_ADDR

test_reset_and_halt_all_harts $NDS_TAP
set test_reset_and_debug_pass "PASS"

#print_info "Test Finished"
# "check changing the JTAG frequency ..."
set test_frequency_pass [test_frequency $NDS_TAP]

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

set regaddr 0xF0100000
set rdata [read_memory_word $NDS_TAP $regaddr] 
puts [format "REG_SMU=0x%x" $rdata]

exit
