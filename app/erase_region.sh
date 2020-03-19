#！/bin/bash

esptool.py --port /dev/ttyUSB0 erase_region 0 0x1F0000
