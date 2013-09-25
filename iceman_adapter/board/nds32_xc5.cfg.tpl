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

proc dma_mww {args} {
	nds mem_access bus
	eval mww $args
	nds mem_access cpu
}

proc dma_mwh {args} {
	nds mem_access bus
	eval mwh $args
	nds mem_access cpu
}

proc dma_mwb {args} {
	nds mem_access bus
	eval mwb $args
	nds mem_access cpu
}

proc dma_mdw {args} {
	nds mem_access bus
	eval mdw $args
	nds mem_access cpu
}

proc dma_mdh {args} {
	nds mem_access bus
	eval mdh $args
	nds mem_access cpu
}

proc dma_mdb {args} {
	nds mem_access bus
	eval mdb $args
	nds mem_access cpu
}

