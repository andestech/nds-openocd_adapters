#
# Andes
#
# http://www.andestech.com
#

#jtag newtap $_CHIPNAME cpu0 -expected-id $_CPUTAPID -irlen 4
#set _TARGETNAME $_CHIPNAME.cpu0
#target create $_TARGETNAME nds32_v3 -endian little -chain-position $_TARGETNAME -coreid 0 -variant $_ACE_CONF

set number_of_core 01

set TARGET_ARCH(0) "nds32_v3"
set TARGET_ARCH(1) "nds32_v3"
set TARGET_ARCH(2) "nds32_v3"
set TARGET_ARCH(3) "nds32_v3"
set TAP_IRLEN(0)  4
set TAP_IRLEN(1)  4
set TAP_IRLEN(2)  4
set TAP_IRLEN(3)  4
set TAP_EXP_CPUID(0)  0x1000063d
set TAP_EXP_CPUID(1)  0x1000063d
set TAP_EXP_CPUID(2)  0x1000063d
set TAP_EXP_CPUID(3)  0x1000063d

for {set i 0} {$i < $number_of_core} {incr i} {
    jtag newtap $_CHIPNAME cpu$i -expected-id $TAP_EXP_CPUID($i) -irlen $TAP_IRLEN($i)
    set _TARGETNAME $_CHIPNAME.cpu$i
    target create $_TARGETNAME $TARGET_ARCH($i) -endian little -chain-position $_TARGETNAME -coreid $i -variant $_ACE_CONF
}
nds runtest_num_clocks 200
