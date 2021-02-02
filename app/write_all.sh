#!/bin/bash

set -Eeuo pipefail

readonly PCNT=3
readonly PRODUCTS=(ExoLed ExoSocket ExoMonsoon)
readonly PARGS=(led socket monsoon)
product=
version=

while getopts "p:v:" opt
do
    case $opt in
        p)
			for ((i=0; i<PCNT; i++)); do
				if [ $OPTARG == ${PARGS[i]} ] ; then
					product=${PRODUCTS[i]}
					product_type=$i
					echo "product=$product"
					break
				fi
			done
			if [ $i == $PCNT ] ; then
				echo "Invalid parameter [-p product] product=led|socket|monsoon"
					exit 1
			fi
			;;
        v)
			if [[ $OPTARG -ge 1 && $OPTARG -le 65535 && $[${OPTARG}%2] == 1 ]] ; then
				version=$OPTARG
				echo "version=$version"
			else
				echo "[optional -v version] 1<=version<=65535 and must be odd, default 1"
				exit 1
			fi
        	;;
        ?)
			echo -e "[-p product] product=led|socket|monsoon\n[optional -v version] 1<=version<=65535, default 1"
			exit 1
			;;
    esac
done

if [ ! $product ] ; then
	echo "Required [-p product] product=led|socket|monsoon"
	exit 1
fi
if [ ! $version ] ; then
	version=1
	echo "Use default version 1"
	echo ""
fi

user_bin=bin/$product/${product}_v${version}.bin
if [ ! -e $user_bin ] ; then
	echo "$user_bin doesn't exist"
	exit 1
fi

echo "flash $user_bin..."

esptool.py -p /dev/ttyUSB0 -b 230400 write_flash --flash_mode qio --flash_freq 40m --flash_size detect 0x0000 ../bin/boot_v1.7.bin 0x1000 $user_bin 0x1fc000 ../bin/esp_init_data_default_v08.bin 0x1fe000 ../bin/blank.bin 0x1f4000 ../tools/bin/esp_ca_cert.bin
