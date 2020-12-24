#！/bin/bash

esptool.py -p /dev/ttyUSB0 -b 230400 write_flash --flash_mode qio --flash_freq 40m --flash_size detect 0x1000 ../bin/upgrade/user1.2048.new.3.bin
