#
# Andes AICE
#
# http://www.andestech.com
#

jtag newtap $_CHIPNAME cpu<_TARGETID> -expected-id $_CPUTAPID

set _TARGETNAME<_TARGETID> $_CHIPNAME.cpu<_TARGETID>
target create $_TARGETNAME<_TARGETID> nds32_<_TARGET_ARCH> -endian little -chain-position $_TARGETNAME<_TARGETID> -coreid <_TARGETID> -variant $_ACE_CONF

set connected<_TARGETID> 0

$_TARGETNAME<_TARGETID> configure -event gdb-attach {
	if  {$connected<_TARGETID> == 0} {
		#<--boot>
		set connected<_TARGETID> 1
		#targets $_TARGETNAME<_TARGETID>
	}
}

$_TARGETNAME<_TARGETID> configure -event gdb-detach {
	if {$connected<_TARGETID> == 1} {
		set connected<_TARGETID> 0
	}
}
