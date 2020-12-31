#!/bin/bash

#set -Eeuxo pipefail
set -Eeuo pipefail

readonly PCNT=3
readonly PRODUCTS=(ExoLed ExoSocket ExoMonsoon)
readonly PARGS=(led socket monsoon)
product=
product_type=
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
			if [ $OPTARG -ge 1 -a $OPTARG -le 65535 ] ; then
				version=$OPTARG
				echo "version=$version"
			else
				echo "[optional -v version] 1<=version<=65535, default 1"
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
	echo "Missing parameter [-p product] product=led|socket|monsoon"
	exit 1
fi
if [ ! $version ] ; then
	version=1
	echo "Use default version 1"
fi

boot=new
app=$[${version}%2]
if [ $app != 1 ]; then
	app=2
fi
spi_speed=40
spi_mode=QIO
spi_size_map=3

echo ""
echo "boot mode: $boot			(0=boot_v1.1, 1=boot_v1.2+, 2=none)"
echo "app=$app					(0=eagle.flash.bin+eagle.irom0text.bin, 1=user1.bin, 2=user2.bin)"
echo "spi speed: $spi_speed MHz	(0=20MHz, 1=26.7MHz, 2=40MHz, 3=80MHz)"
echo "spi mode: $spi_mode		(0=QIO, 1=QOUT, 2=DIO, 3=DOUT)"
echo "spi_map: $spi_size_map"

touch user/user_main.c

echo ""
echo "start..."
echo ""

make COMPILE=gcc BOOT=$boot APP=$app SPI_SPEED=$spi_speed SPI_MODE=$spi_mode SPI_SIZE_MAP=$spi_size_map PRODUCT_TYPE=$product_type FIRMWARE_VERSION=$version

if [ $? != 0 ] ; then
	exit 1
fi

# compile_time=$(date +%s%3N)
compile_time=$(date "+%Y-%m-%d %H:%M:%S")
echo "compile time: $compile_time"

echo ""
if [ ! -d "bin/$product" ]; then
	echo "create folder bin/$product"
	mkdir -p bin/$product
else
	echo "folder bin/$product already exist"
fi

echo ""
echo "Copy ../bin/upgrade/user$app.2048.new.$spi_size_map.bin to bin/$product/${product}_v${version}.bin"
echo "************************************************"
echo ""
cp -R -f ../bin/upgrade/user$app.2048.new.$spi_size_map.bin bin/$product/${product}_v${version}.bin
