source [find nds_custom_reset_func.tcl]
set NDS_TAP "123"
scan [jtag names] "%s" NDS_TAP

set NDS_DMI_ADDR	    0
set NDS_DMI_DATA	    0
scan [nds dmi_addr] "%x" NDS_DMI_ADDR
scan [nds dmi_data] "%x" NDS_DMI_DATA
#puts [format "dmi-write = 0x%x, 0x%x" $NDS_DMI_ADDR $NDS_DMI_DATA]


# Reset the Debug Module
test_reset_and_debug $NDS_TAP
