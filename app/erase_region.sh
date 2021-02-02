#!/bin/bash

esptool.py -p /dev/ttyUSB0 -b 230400 erase_region 0 0x1F0000
