source [find interface/nds32-aice.cfg]
source [find board/nds32_xc5.cfg]

gdb_target_description enable
gdb_report_data_abort enable

proc dma_mwd {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	eval mwd $args

	nds mem_access [string tolower $backup]
}

proc dma_mww {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	eval mww $args

	nds mem_access [string tolower $backup]
}

proc dma_mwh {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	eval mwh $args

	nds mem_access [string tolower $backup]
}

proc dma_mwb {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	eval mwb $args

	nds mem_access [string tolower $backup]
}

proc dma_mdd {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	echo [eval mdd $args]

	nds mem_access [string tolower $backup]
}

proc dma_mdw {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	echo [eval mdw $args]

	nds mem_access [string tolower $backup]
}

proc dma_mdh {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	echo [eval mdh $args]

	nds mem_access [string tolower $backup]
}

proc dma_mdb {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	echo [eval mdb $args]

	nds mem_access [string tolower $backup]
}

proc dma_read_buffer {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	echo [eval nds read_buffer $args]

	nds mem_access [string tolower $backup]
}

proc dma_write_buffer {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	eval nds write_buffer $args

	nds mem_access [string tolower $backup]
}

proc dma_mdb_w {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	# get parameters
	set address [lindex $args 0]
	set count [lindex $args 1]

	set aligned [expr {$address & 0xFFFFFFFC}]
	set offset [expr {$address & 0x3}]
	eval mem2array wordarray 32 $aligned 1
	set answer [expr {$wordarray(0) >> ($offset * 8)}]
	set answer [expr {$answer & 0xFF}]
	set answer_byte [format %02X $answer]
	set answer_addr [format %08X $address]
	puts "0x$answer_addr: $answer_byte"

	nds mem_access [string tolower $backup]
}

proc dma_mwb_w {args} {
	set val [nds mem_access]
	set backup [lindex $val 4]
	nds mem_access bus

	# get parameters
	set address [lindex $args 0]
	set data [lindex $args 1]

	set aligned [expr {$address & 0xFFFFFFFC}]
	set offset [expr {$address & 0x3}]

	eval mem2array wordarray 32 $aligned 1

	set mask [expr {0xFF << ($offset * 8)}]
	set mask [expr {~ $mask}]
	set wordarray(0) [expr {$wordarray(0) & $mask}]

	set data [expr {$data << ($offset * 8)}]
	set wordarray(0) [expr {$wordarray(0) | $data}]

	eval array2mem wordarray 32 $aligned 1

	nds mem_access [string tolower $backup]
}



