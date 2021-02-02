#!/bin/bash

esptool.py -p /dev/ttyUSB0 -b 230400 erase_flash
