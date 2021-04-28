
# Main
source [find openocd.cfg]
source [find debug_util.tcl]
set verbosity 100
set time_target_sec 1
#adapter_khz 10000
#source [find interface/olimex-arm-usb-tiny-h.cfg]

# ABSTRCT_ERR:
# 0 : no error
# 1 : error
set ABSTRCT_ERR 0

# Define
set CSR_MVENDORID  0xf11
set CSR_MARCHID    0xf12
set CSR_MIMPID     0xf13
set CSR_MISA       0x301
set CSR_DCSR       0x7b0
set CSR_MNVEC      0x7c3

set CHECK_SMU 0


proc single_step_set_dcsr {tap xlen} {
	global CSR_DCSR
	set dcsr [read_register $tap $xlen $CSR_DCSR]
	set STEP_MASK [expr 1 << 2]
	set EBREAKU_MASK [expr 1 << 12]
	set EBREAKS_MASK [expr 1 << 13]
	set EBREAKM_MASK [expr 1 << 15]
	set dcsr [expr $dcsr | $STEP_MASK | $EBREAKU_MASK | $EBREAKS_MASK | $EBREAKM_MASK]
	write_register $tap $xlen $CSR_DCSR $dcsr
}

proc maybe_execute_fence {tap hartstart hartmax currenthart} {
	# current hart
	execute_fence $tap
	# other harts
	for {set hartsel $hartstart} {$hartsel < $hartmax} {incr $hartsel} {
		if {[expr $hartsel == $currenthart]} {
			continue
		}
		if {[select_single_hart $tap $hartsel]} {
			continue
		}
		if {[is_selected_hart_anyunavail $tap]} {
			continue
		}
		halt_hart $tap $hartsel
		execute_fence $tap
	}
	# restore original hart
	if {[select_single_hart $tap $currenthart]} {
		puts [format "select original core %d failed", $currenthart]
	}
}

proc abstractauto_run {tap abstractauto testvar abstractcs} {
	write_dmi_abstractauto $tap 0xFFFFFFFF
	set autoexecdata [expr $abstractauto & 0xfff]
	set autoexecprogbuf [expr ($abstractauto >> 16) & 0xffff]
	set abs_comd [read_dmi_abstractcommand $tap]
	print_debug_msg [format "command 0x%08x" $abs_comd]
	for {set dc 0} {$dc < 12} {incr $dc} {
		set data [read_dmi_abstractdata $tap $dc]
		while 1 {
			if {![is_abstractcs_busy $tap]} {
				break
			}
		}
		if {[expr $autoexecdata & (1 << $dc)]} {
			print_debug_msg [format "data%d: 0x%08x" $dc $data]
			set datacount [expr $abstractcs & 0xf]
			assert {$dc < $datacount} [format "data%d > data count%d" $dc $datacount]
			set testvar [expr $testvar + 1]
		}
	}
	for {set pbc 0} {$pbc < 16} {incr $pbc} {
		set data [read_dmi_progbuf $tap $pbc]
		while 1 {
			if {![is_abstractcs_busy $tap]} {
				break
			}
		}
		if {[expr $autoexecprogbuf & (1 << $pbc)]} {
			print_debug_msg [format "progbuf%d: 0x%08x" $pbc $data]
			set progbufsize [expr ($abstractcs >> 24) & 0x1f]
			assert {$pbc < $progbufsize} [format "progbuf%d > progbufsize%d" $pbc $progbufsize]
			set testvar [expr $testvar + 1]
		}
	}
	write_dmi_abstractauto $tap 0x0
	return $testvar
}

proc test_memory_rw {tap hart start_addr} {
	set count 4
	set wdata_list {0xaaaaaaaa 0xbbbbbbbb 0xcccccccc 0xdddddddd}
	global ABSTRCT_ERR
	puts [format "Testing memory write from addr = 0x%x, size:4 words" $start_addr]
	for {set i 0} {$i < $count} {incr i} {
		set addr [expr $start_addr + $i*4]
		set wdata [lindex $wdata_list $i]
		write_memory_word $tap $addr $wdata
		if {$ABSTRCT_ERR} {
			print_error_msg [format "core%d: write memory(address:0x%x) error" $hart $addr]
			return "NG"
		}
	}

	puts [format "Testing memory read from addr = 0x%x, size:4 words" $start_addr]
	for {set i 0} {$i < $count} {incr i} {
		set addr [expr $start_addr + $i*4]
		set wdata [lindex $wdata_list $i]

		set rdata [read_memory_word $tap $addr]
		if {$ABSTRCT_ERR} {
			print_error_msg [format "core%d: read memory(address:0x%x) error" $hart $addr]
			return "NG"
		}
		#assert {$rdata == $wdata } [format "read/write memory mismatch: addr=0x%x, wdata=0x%x, rdata=0x%x" $addr $wdata $rdata]
		if [ expr $rdata != $wdata ] {
			print_error_msg [format "core%d: read/write memory mismatch: addr=0x%x, wdata=0x%x, rdata=0x%x" $hart $addr $wdata $rdata]
			return "NG"
		}
	}
	return "PASS"
}

proc test_abstract_memory_rw {tap hart start_addr xlen} {
	set wdata_list {0xaaaaaaaaaaaaaaaa 0xbbbbbbbbbbbbbbbb 0xcccccccccccccccc 0xdddddddddddddddd}
	puts [format "Testing abstract block memory write from addr 0x%x, size:4 words" $start_addr]
	if {![abstract_write_block_memory $tap $xlen $start_addr $wdata_list]} {
		print_error_msg [format "core%d: write memory error" $hart]
		return "NG"
	}
	puts [format "Testing abstract block memory read from addr 0x%x, size:4 words" $start_addr]
	if {![abstract_read_block_memory $tap $xlen $start_addr $wdata_list]} {
		print_error_msg [format "core%d: read memory error" $hart]
		return "NG"
	}
	return "PASS"
}

proc reset_test_pre_setting {tap debug_buffer_size debug_data_size} {
		write_dmi_abstractcommand $tap 0xffffffff
		for {set c 0} {$c < $debug_buffer_size} {incr $c} {
			set testvar [expr ($c + 1) * 0x11111111]
			write_dmi_progbuf $tap $c $testvar
		}
		for {set c 0} {$c < $debug_data_size} {incr $c} {
			set testvar [expr ($c + 1) * 0x11111111]
			write_dmi_abstractdata $tap $c $testvar
		}
		write_dmi_abstractauto $tap 0xFFFFFFFF
		return [read_dmi_abstractauto $tap]
}

proc after_ndm_reset_check {tap abstractauto debug_buffer_size debug_data_size} {
	puts "execute ndmreset should not affect these registers:abstractcs, abstractauto, program buffer and abstract data"
	set abstractcs [read_dmi_abstractcs $tap]
	set cmderr [expr ($abstractcs >> 8) & 0x7]
	if {$cmderr != 2} {
		print_error_msg [format "mismatch value of abstractcs.cmderr: before NDMRESET:2, after NDMRESET:%d" $cmderr]
		return 1
	}
	set now_abstractauto [read_dmi_abstractauto $tap]
	if {$now_abstractauto != $abstractauto} {
		print_error_msg [format "mismatch value of abstractauto: before NDMRESET:0x%x, after NDMRESET:0x%x" $abstractauto $now_abstractauto]
		return 1
	}
	# Clean up to avoid future test failures
	# clear abstractcs.cmderr
	puts "clear abstractcs.cmderr(W1C) and abstractauto(0)"
	write_dmi_abstractcs $tap [expr 0x7 << 8]
	write_dmi_abstractauto $tap 0x0

	for {set c 0} {$c < $debug_buffer_size} {incr $c} {
		set testvar [expr ($c + 1) * 0x11111111]
		set data [read_dmi_progbuf $tap $c]
		if {$data != $testvar} {
			print_error_msg [format "mismatch value of program buffer %d: before NDMRESET:0x%x, after NDMRESET:0x%x" $c $testvar $data]
			return 1
		}
	}
	for {set c 0} {$c < $debug_data_size} {incr $c} {
		set testvar [expr ($c + 1) * 0x11111111]
		set data [read_dmi_abstractdata $tap $c]
		if {$data != $testvar} {
			print_error_msg [format "mismatch value of abstract data %d: before NDMRESET:0x%x, after NDMRESET:0x%x" $c $testvar $data]
			return 1
		}
	}
	return 0
}

proc test_reset_and_halt_all_harts {tap hartstart hartcount debug_buffer_size debug_data_size} {
	global CSR_DCSR
	set abstractauto [reset_test_pre_setting $tap $debug_buffer_size $debug_data_size]

	if {$hartcount == 1} {
		puts "Testing reset_and_halt_one_hart"
		set result [reset_and_halt_one_hart $tap $hartstart]
		if {$result} {
			return "NG"
		}
	} else {
		puts "Testing reset_and_halt_all_harts"
		reset_and_halt_all_harts $tap $hartstart $hartcount
	}

	if {[after_ndm_reset_check $tap $abstractauto $debug_buffer_size $debug_data_size]} {
		return "NG"
	}

	set hartmax [expr $hartstart + $hartcount]
	# wait 1 sconds
	after 1000
	global CSR_MNVEC
	global scan_hart_nums
	global NDS_TARGETNAME
	set ok_cores 0
	for {set hartsel $hartstart} {$hartsel < $hartmax} {incr $hartsel} {
		if {[select_single_hart $tap $hartsel]} {
			puts [format "select core %d failed", $hartsel]
			continue
		}
		if {[is_selected_hart_anyunavail $tap]} {
			puts [format "core%d is unavailable" $hartsel]
			continue
		}

		scan [nds target_xlen] "%x" hartxlen
		set pc [read_dpc $tap $hartxlen]
		puts [format "Hart %d pc = 0x%x" $hartsel $pc]
		set mnvec [read_register $tap $hartxlen $CSR_MNVEC]
		if [ expr $pc != $mnvec ] {
			print_error_msg [format "core%d: pc != mnvec0x%x" $hartsel $mnvec]
			return "NG"
		}
		set dcsr [read_register $tap $hartxlen $CSR_DCSR]
		set CAUSE_MASK 0x1C0
		set cause_reason [expr (($dcsr & $CAUSE_MASK) >> 6)]
		array set str_cause {0 reserved 1 software-breakpoint 2 trigger-module 3 haltreq 4 single-step 5 resethaltreq}
		if {$cause_reason != 5 && $cause_reason != 3} {
			print_error_msg [format "core%d: Incorrect register dcsr.cause=%d(%s), expected 3(haltreq) or 5(resethaltreq)" $hartsel $cause_reason $str_cause($cause_reason)]
			return "NG"
		}
		set ok_cores [expr $ok_cores + 1]
	}
	if {$ok_cores > 0} {
		return "PASS"
	} else {
		return "NG"
	}
}

proc after_dm_reset_check {tap debug_buffer_size debug_data_size} {
	puts "execute DM reset should reset these registers:abstractcs, abstractauto, program buffer and abstract data"
	set abstractcs [read_dmi_abstractcs $tap]
	set cmderr [expr ($abstractcs >> 8) & 0x7]
	if {$cmderr != 0} {
		print_error_msg [format "Incorrect register abstractcs.cmderr=%d, expected 0" $cmderr]
		puts "clear abstractcs.cmderr(W1C)"
		write_dmi_abstractcs $tap [expr 0x7 << 8]
		return 1
	}
	set now_abstractauto [read_dmi_abstractauto $tap]
	if {$now_abstractauto != 0} {
		print_error_msg [format "Incorrect register abstractauto=0x%x, expected 0" $now_abstractauto]
		puts "clear abstractauto(0)"
		write_dmi_abstractauto $tap 0x0
		return 1
	}

	for {set c 0} {$c < $debug_buffer_size} {incr $c} {
		set data [read_dmi_progbuf $tap $c]
		if {$data != 0} {
			print_error_msg [format "Incorrect program buffer %d=0x%x, expected 0" $c $data]
			return 1
		}
	}
	for {set c 0} {$c < $debug_data_size} {incr $c} {
		set data [read_dmi_abstractdata $tap $c]
		if {$data != 0} {
			print_error_msg [format "Incorrect abstract data %d=0x%x, expected 0" $c $data]
			return 1
		}
	}
	return 0
}

proc test_reset_dm_related {tap debug_buffer_size debug_data_size} {
	reset_test_pre_setting $tap $debug_buffer_size $debug_data_size
	reset_dm $tap
	return [after_dm_reset_check $tap $debug_buffer_size $debug_data_size]
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

	if [ expr $test2_adapter_khz != $test_adapter_khz ] {
		print_error_msg "Unable to set adapter speed"
		set nds_if_pass "NG"
	}
	# Restore cur_speed
	nds adapter_khz $cur_adapter_khz

	return $nds_if_pass
}

proc get_march_name {marchid} {
	#set arch_name_list {"N" "D" "E" "S" "" "" "" "" "NX" "DX" "EX" "SX"}
	#set name_idx [expr ($marchid>>12)&0xf]
	#set march_name [lindex $arch_name_list $name_idx]
	#set march_name_out [format "%s%d%d" $march_name [expr ($marchid>>4)&0xf] [expr ($marchid>>0)&0xf] ]
	##puts [format "march_name_1 %s" $march_name_out]
	set arch_name_list {"N" "" "" "" "FS" "" "" "" "" "" "A" "" "" "D"}
	set name_idx [expr ($marchid>>8)&0xF]
	set march_name [lindex $arch_name_list $name_idx]
	if { [expr ($marchid>>15)&0x1] == 1} {
		set march_name_out [format "%sX%d%d" $march_name [expr ($marchid>>4)&0xf] [expr ($marchid>>0)&0xf] ]
	} else {
		set march_name_out [format "%s%d%d" $march_name [expr ($marchid>>4)&0xf] [expr ($marchid>>0)&0xf] ]
	}
	return $march_name_out
}

proc get_platform_name {platformid} {
	set platform_name_out [format "%c%c%d%d%d" [expr ($platformid>>24)&0xff] [expr ($platformid>>16)&0xff] [expr ($platformid>>12)&0xf] [expr ($platformid>>8)&0xf] [expr ($platformid>>4)&0xf]]
	return $platform_name_out
}

proc parsing_targets {} {
	global NDS_TARGETS_COUNT
	global NDS_TARGETS_NAME
	global NDS_TARGETS_COREID
	global NDS_TARGETS_CORENUMS

	scan [nds target_count] "%x" NDS_TARGETS_COUNT
	if {$NDS_TARGETS_COUNT == 0} {
		print_error_msg "the number of targets should not be 0"
		exit
	}
	for {set i 0} {$i < $NDS_TARGETS_COUNT} {incr $i} {
		scan [nds target_info $i] "%s %x %x" NDS_TARGETS_NAME($i) NDS_TARGETS_COREID($i) NDS_TARGETS_CORENUMS($i)
		#puts [format "target-%d = %s 0x%x 0x%x" $i $NDS_TARGETS_NAME($i) $NDS_TARGETS_COREID($i) $NDS_TARGETS_CORENUMS($i)]
	}
	for {set i 0} {$i < $NDS_TARGETS_COUNT} {incr $i} {
		puts [format "target-%d = %s 0x%x 0x%x" $i $NDS_TARGETS_NAME($i) $NDS_TARGETS_COREID($i) $NDS_TARGETS_CORENUMS($i)]
	}
	return
}

parsing_targets

set time_start [clock seconds]
set time_end   [clock seconds]
while {[expr $time_end-$time_start] < $time_target_sec} {
	set test_dm_operate_pass "SKIP"
	set test_dtm_connect_pass "SKIP"
	set test_frequency_pass "SKIP"
	set test_gpr_access_pass "SKIP"
	set test_abstractauto_pass "SKIP"
	set test_single_step_pass "SKIP"
	set test_memory_access_pass "SKIP"
	set test_haltsum0_pass "SKIP"
	set test_haltsum1_pass "SKIP"
	set test_reset_and_debug_pass "SKIP"
	init
	set targetcount $NDS_TARGETS_COUNT
	for {set i 0} {$i < $targetcount} {incr $i} {
		targets $NDS_TARGETS_NAME($i)
		scan [nds jtag_tap_name] "%s" NDS_TAP
		puts [format "== TEST target-%d ==" $i]

		puts "* Initializing Debug Module Interface"
		init_dmi $NDS_TAP

		puts "* Initializing Debug Module"
		reset_dm $NDS_TAP
		reset_ndm $NDS_TAP
		set test_dm_operate_pass "PASS"

		puts "Scanning number of harts"
		set scan_hart_nums [scan_harts $NDS_TAP]
		print_debug_msg [format "scan_hart_nums=0x%x" $scan_hart_nums]
		if {[ expr $scan_hart_nums > 0 ]} {
			if {$test_dtm_connect_pass != "NG"} {
				set test_dtm_connect_pass "PASS"
			}
		} else {
			set test_dtm_connect_pass "NG"
			print_error_msg "the number of harts should not be 0"
			continue
		}

		puts "* test frequency"
		if {[test_frequency $NDS_TAP] == "PASS" && $test_frequency_pass != "NG"} {
			set test_frequency_pass "PASS"
		} else {
			set test_frequency_pass "NG"
		}

		set expected_haltharts0 0
		set expected_haltharts1 0
		set hartcount $NDS_TARGETS_CORENUMS($i)
		set hartmax [expr $NDS_TARGETS_COREID($i) + $hartcount]
		for {set hartsel $NDS_TARGETS_COREID($i)} {$hartsel < $hartmax} {incr $hartsel} {
			if {[select_single_hart $NDS_TAP $hartsel]} {
				continue
			}
			if {[is_selected_hart_anyunavail $NDS_TAP]} {
				puts [format "core%d is unavailable" $hartsel]
				continue
			}
			puts [format "Halting Hart %d" $hartsel]
			halt_hart $NDS_TAP $hartsel

			if [expr $hartsel < 32] {
				set expected_haltharts0 [expr ($expected_haltharts0 | (1 << $hartsel))]
			}
			if [expr $hartsel < 1024] {
				set expected_haltharts1 [expr ($expected_haltharts1 | (1 << ($hartsel/32)))]
			}

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
			set abstractcs [read_dmi_abstractcs $NDS_TAP]
			set debug_buffer_size [expr ($abstractcs >> 24) & 0x1f]
			puts [format "core%d: debug_buffer_size=0x%x" $hartsel $debug_buffer_size]

			# Access GPRs
			puts "* Access GPRs testing"
			for {set gpr_n 1} {$gpr_n < 32} {set gpr_n [expr $gpr_n << 1]} {
				set GPR_Addr [expr 0x1000 + $gpr_n]
				set testval [expr ($gpr_n | ($gpr_n << 32))]
				write_register $NDS_TAP $hartxlen $GPR_Addr $testval
				set reset_data 0xDEADBEEFDEADBEEF
				write_dmi_abstractdata $NDS_TAP 0 [expr $reset_data & 0xFFFFFFFF]
				if {$hartxlen == 64} {
					write_dmi_abstractdata $NDS_TAP 1 [expr $reset_data >> 32]
				}
				set GPR_Value [read_register $NDS_TAP $hartxlen $GPR_Addr]
				if {$hartxlen == 64} {
					if {$GPR_Value != $testval} {
						set test_gpr_access_pass "NG"
						print_error_msg [format "core%d: GPR%d read/write mismatch: rdata=0x%x, wdata=0x%x" $hartsel $gpr_n $GPR_Value $testval]
					} elseif {$test_gpr_access_pass != "NG"} {
						set test_gpr_access_pass "PASS"
					}
				} else {
					set cmp_value [expr $testval & 0xFFFFFFFF]
					if {$GPR_Value != $cmp_value} {
						set test_gpr_access_pass "NG"
						print_error_msg [format "core%d: GPR%d read/write mismatch: rdata=0x%x wdata=0x%x" $hartsel $gpr_n $GPR_Value $cmp_value]
					} elseif {$test_gpr_access_pass != "NG"} {
						set test_gpr_access_pass "PASS"
					}
				}
			}

			# abstractauto
			puts "* Abstractauto testing"
			write_dmi_abstractauto $NDS_TAP 0xFFFFFFFF
			set abstractauto [read_dmi_abstractauto $NDS_TAP]
			write_dmi_abstractauto $NDS_TAP 0x0
			if {$abstractauto > 0} {
				if {$debug_buffer_size >= 3} {
					set testvar 0
					set GPRS0_Addr [expr 0x1000 + 8]
					write_register $NDS_TAP $hartxlen $GPRS0_Addr $testvar
					abstractauto_addi $NDS_TAP
					set testvar [expr $testvar + 1]
					set testvar [abstractauto_run $NDS_TAP $abstractauto $testvar $abstractcs]
					set fin_value [read_register $NDS_TAP $hartxlen $GPRS0_Addr]
					if {$fin_value != $testvar} {
						set test_abstractauto_pass "NG"
						print_error_msg [format "core%d: Incorrect register abstractauto=%d, expected %d" $hartsel $fin_value $testvar]
					} elseif {$test_abstractauto_pass != "NG"} {
						set test_abstractauto_pass "PASS"
					}
				} else {
					puts [format "core%d: progbuf size < 3, skip abstractauto test" $hartsel]
				}
			} else {
				puts [format "core%d: cannot write abstractauto(value 0), skip abstractauto test" $hartsel]
			}


			# Single Step
			puts "* Single Step testing"
			set dmstatus [read_dmi_dmstatus $NDS_TAP]
			set dmstatus_impebreak [expr ($dmstatus>>22)&0x1]
			if [ expr ($debug_buffer_size + $dmstatus_impebreak) >= 3 ] {
				maybe_execute_fence $NDS_TAP $NDS_TARGETS_COREID($i) $hartmax $hartsel
			}
			# set dcsr
			single_step_set_dcsr $NDS_TAP $hartxlen
			# resume setting
			resume_hart_on_step $NDS_TAP $hartsel
			# check the halt reason
			set new_dcsr [read_register $NDS_TAP $hartxlen $CSR_DCSR]
			set CAUSE_MASK 0x1C0
			set cmp_cause [expr ($new_dcsr & $CAUSE_MASK) >> 6]
			if {$cmp_cause != 4} {
				print_error_msg [format "core%d: Incorrect register dcsr.cause=%d expected 4(single step)" $hartsel $cmp_cause]
				set pc [read_dpc $NDS_TAP $hartxlen]
				puts [format "core%d pc = 0x%x" $hartsel $pc]
				set test_single_step_pass "NG"
			} elseif {$test_single_step_pass != "NG"} {
				set test_single_step_pass "PASS"
			}
			# restore dcsr
			write_register $NDS_TAP $hartxlen $CSR_DCSR $dcsr

			if {$CHECK_SMU != 0} {
				set regaddr 0xF0100000
				if [ expr $debug_buffer_size > 7 ] {
					set rdata [read_memory_word $NDS_TAP $regaddr]
					if {$ABSTRCT_ERR} {
						puts [format "core%d: read SMU failed, maybe testing board's SMU register addrress is not 0x%x" $hartsel $regaddr]
					}
				} else {
					set rdata [expr [abstract_read_memory $NDS_TAP $hartxlen $regaddr] & 0xFFFFFFFF]
				}
				if {$ABSTRCT_ERR == 0} {
					set platform_name [get_platform_name $rdata]
					puts [format "REG_SMU=0x%x %s" $rdata $platform_name]
				}
			}

			if {$NDS_MEM_TEST == 1} {
				puts "* access memory"
				if [ expr $debug_buffer_size > 7 ] {
					if {[test_memory_rw $NDS_TAP $hartsel $NDS_MEM_ADDR] == "PASS" && $test_memory_access_pass != "NG"} {
						set test_memory_access_pass "PASS"
					} else {
						set test_memory_access_pass "NG"
					}
				} else {
					if {[test_abstract_memory_rw $NDS_TAP $hartsel $NDS_MEM_ADDR $hartxlen] == "PASS" && $test_memory_access_pass != "NG"} {
						set test_memory_access_pass "PASS"
					} else {
						set test_memory_access_pass "NG"
					}
				}
			}
		}

		puts "* Check haltsum0 value"
		set haltsum0 [read_dmi_haltsum0 $NDS_TAP]
		if [expr $haltsum0 != $expected_haltharts0] {
			set test_haltsum0_pass "NG"
			print_error_msg [format "Incorrect register haltsum0=%d, expected %d." $haltsum0 $expected_haltharts0]
		} elseif {$test_haltsum0_pass != "NG"} {
			set test_haltsum0_pass "PASS"
		}
		puts "* Check haltsum1 value"
		set haltsum1 [read_dmi_haltsum1 $NDS_TAP]
		if [expr $haltsum1 != $expected_haltharts1] {
			if {$hartcount < 33} {
				puts "This register haltsum1 may not be present in systems with fewer than 33 harts."
			} else {
				set test_haltsum1_pass "NG"
				print_error_msg [format "Incorrect register haltsum1=%d, expected %d." $haltsum1 $expected_haltharts1]
			}
		} elseif {$test_haltsum1_pass != "NG"} {
			set test_haltsum1_pass "PASS"
		}

		set hartsel $NDS_TARGETS_COREID($i)
		set abstractcs [read_dmi_abstractcs $NDS_TAP]
		set debug_buffer_size [expr ($abstractcs >> 24) & 0x1f]
		set debug_data_size [expr $abstractcs & 0xf]
		print_debug_msg [format "debug_buffer_size=0x%x debug_data_size=0x%x" $debug_buffer_size $debug_data_size]
		# ndmreset
		if {[test_reset_and_halt_all_harts $NDS_TAP $hartsel $hartcount $debug_buffer_size $debug_data_size] == "PASS" && $test_reset_and_debug_pass != "NG"} {
			set test_reset_and_debug_pass "PASS"
		} else {
			set test_reset_and_debug_pass "NG"
		}
		# dmactive
		if {[test_reset_dm_related $NDS_TAP $debug_buffer_size $debug_data_size] == 0 && $test_reset_and_debug_pass != "NG"} {
			set test_reset_and_debug_pass "PASS"
		} else {
			set test_reset_and_debug_pass "NG"
		}
	}

	# dmi_busy_delay_count default value: 3
	set default_delay 3
	set max_delay $default_delay
	if {$NDS_MEM_TEST == 1 && $test_memory_access_pass == "PASS"} {
		puts [format "write 4 words from memory:0x%x to get dmi_busy_delay_count" $NDS_MEM_ADDR]
		nds configure scan_retry_times 3
		nds configure jtag_scans_optimize 4
		nds configure jtag_max_scans 64
		mww $NDS_MEM_ADDR 1 4
		scan [nds get_dmi_delay] "%d" delay_count
		if {$delay_count > $max_delay} {
			set max_delay $delay_count
			print_debug_msg [format "cpu mode dmi_busy_delay_count:%d" $delay_count]
		}
		#bus mode mww
		dma_mww $NDS_MEM_ADDR 1 4
		scan [nds get_dmi_delay] "%d" delay_count
		if {$delay_count > $max_delay} {
			set max_delay $delay_count
			print_debug_msg [format "bus mode dmi_busy_delay_count:%d" $delay_count]
		}
	}

	puts [format "********************"]
	puts [format "Diagnostic Report"]
	puts [format "********************"]
	if {$test_dm_operate_pass == "PASS" || $test_dm_operate_pass == "NG"} {
		puts [format "(%s) check that Debug Module (DM) is operational ..." $test_dm_operate_pass]
	}
	if {$test_dtm_connect_pass == "PASS" || $test_dtm_connect_pass == "NG"} {
		puts [format "(%s) check JTAG/DTM connectivity ..." $test_dtm_connect_pass]
	}
	if {$test_frequency_pass == "PASS" || $test_frequency_pass == "NG"} {
		puts [format "(%s) check changing the JTAG frequency ..." $test_frequency_pass]
	}
	if {$test_gpr_access_pass == "PASS" || $test_gpr_access_pass == "NG"} {
		puts [format "(%s) check GPRs access ..." $test_gpr_access_pass]
	}
	if {$test_abstractauto_pass == "PASS" || $test_abstractauto_pass == "NG"} {
		puts [format "(%s) check ABSTRACTAUTO should cause COMMAND to run the expected number of times ..." $test_abstractauto_pass]
	}
	if {$test_single_step_pass == "PASS" || $test_single_step_pass == "NG"} {
		puts [format "(%s) check all harts single step ..." $test_single_step_pass]
	}
	if {$test_memory_access_pass == "PASS" || $test_memory_access_pass == "NG"} {
		puts [format "(%s) check accessing memory through CPU ..." $test_memory_access_pass]
	}
	if {$test_haltsum0_pass == "PASS" || $test_haltsum0_pass == "NG"} {
		puts [format "(%s) check haltsum0 ..." $test_haltsum0_pass]
	}
	if {$test_haltsum1_pass == "PASS" || $test_haltsum1_pass == "NG"} {
		puts [format "(%s) check haltsum1 ..." $test_haltsum1_pass]
	}
	if {$test_reset_and_debug_pass == "PASS" || $test_reset_and_debug_pass == "NG"} {
		puts [format "(%s) check reset-and-halt ..." $test_reset_and_debug_pass]
	}
	if {$max_delay > $default_delay} {
		puts [format "suggest starting ICEman with --dmi_busy_delay_count %d" $max_delay]
	}
	puts [format "********************"]

	set time_end [clock seconds]
}

exit
