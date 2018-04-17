
# main
adapter_khz 10000
source [find interface/olimex-arm-usb-tiny-h.cfg]

set verbosity 100
source [find debug_util.tcl]

proc test_memory_rw {} {
	set start_addr 0x0
	set count 4
	set wdata_list {0xaaaaaaaa 0xbbbbbbbb 0xcccccccc 0xdddddddd}

	print_info "Testing memory write"
	for {set i 0} {$i < $count} {incr i} {
		set addr [expr $start_addr + $i*4]
		set wdata [lindex $wdata_list $i]

		write_memory_word target.dtm $addr $wdata 
	}
	
	print_info "Testing memory read"
	for {set i 0} {$i < $count} {incr i} {
		set addr [expr $start_addr + $i*4]
		set wdata [lindex $wdata_list $i]

		set rdata [read_memory_word target.dtm $addr]
		assert {$rdata == $wdata } [format "read/write memory mismatch: addr=0x%x, wdata=0x%x, rdata=0x%x" $addr $wdata $rdata]
	}

}

proc test_reset_and_halt_all_harts {} {
	print_info "Testing reset_and_halt_all_harts"
	reset_and_halt_all_harts target.dtm
	# wait 1 sconds
	after 1000
	scan_harts target.dtm

	set hartsel 0
	set hartxlen 32

	select_single_hart target.dtm $hartsel
	set pc [read_dpc target.dtm $hartxlen]

	print_info [format "Hart %d pc = 0x%x" $hartsel $pc]
}

jtag newtap target dtm -irlen 5 -expected-id 0x1000563D

init

print_debug_msg "Initializing Debug Module Interface"
init_dmi target.dtm

print_debug_msg "Initializing Debug Module"
reset_dm target.dtm
reset_ndm target.dtm

print_debug_msg "Scanning number of harts"
scan_harts target.dtm

print_info "Halting Hart 0"
set hartsel 0
set hartxlen 32
halt_hart target.dtm $hartsel

set mvendorid [read_register target.dtm $hartxlen 0xf11]
print_info [format "mvendorid=0x%x" $mvendorid]
set marchid [read_register target.dtm $hartxlen 0xf12]
print_info [format "marchid=0x%x" $marchid]
set mimpid [read_register target.dtm $hartxlen 0xf13]
print_info [format "mimpid=0x%x" $mimpid]

test_memory_rw

test_reset_and_halt_all_harts

print_info "Test Finished"

exit
