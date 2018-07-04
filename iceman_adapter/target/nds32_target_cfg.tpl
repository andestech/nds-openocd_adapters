#
# Andes
#
# http://www.andestech.com
#

jtag newtap $_CHIPNAME cpu0 -expected-id $_CPUTAPID -irlen 4
set _TARGETNAME $_CHIPNAME.cpu0
target create $_TARGETNAME nds32_v3 -endian little -chain-position $_TARGETNAME -coreid 0 -variant $_ACE_CONF
