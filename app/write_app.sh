#！/bin/bash

esptool.py --port /dev/ttyUSB0 -b 230400 write_flash 0x1000 ../bin/upgrade/user1.2048.new.3.bin
