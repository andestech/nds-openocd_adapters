#
# Andes AICE
#
# http://www.andestech.com
#

interface aice
aice desc "Andes AICE adapter"
aice desc_vid 0 "Andes"
aice desc_pid 0 "AICE"
aice desc_pid 1 "AICE-MCU"
aice desc_pid 2 "AICE-MINI"
aice serial "C001-42163"
aice vid_pid 0x1CFC 0x0000
#aice port aice_pipe
#aice adapter aice_adapter
aice port aice_usb
reset_config trst_and_srst
