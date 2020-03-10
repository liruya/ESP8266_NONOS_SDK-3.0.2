#！/bin/bash

esptool.py --port /dev/ttyUSB0 -b 230400 write_flash 0x1f4000 ../tools/bin/esp_ca_cert.bin
