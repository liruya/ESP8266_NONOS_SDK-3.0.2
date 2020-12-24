#！/bin/bash

esptool.py -p /dev/ttyUSB0 -b 230400 write_flash --flash_mode qio --flash_freq 40m --flash_size detect 0x0000 ../bin/boot_v1.7.bin 0x1000 ../bin/upgrade/user1.2048.new.3.bin 0x1fc000 ../bin/esp_init_data_default_v08.bin 0x1fe000 ../bin/blank.bin 0x1f4000 ../tools/bin/esp_ca_cert.bin
