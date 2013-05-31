set _CPUTAPID 0x1000063d
set _CHIPNAME nds32
#--target

jtag init
#--edm-passcode
#--soft-reset-halt

set connected 0

$_TARGETNAME configure -event gdb-attach {
	if  {$connected == 0} {
		#--boot [reset halt]
		set connected 1
	}
}

$_TARGETNAME configure -event gdb-detach {
	if {$connected == 1} {
		set connected 0
	}
}
