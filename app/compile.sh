#!/bin/bash

echo "gen_misc.sh version 20150511"
echo ""

product_list=(ExoLed ExoSocket ExoMonsoon)

echo "Please select product:"
echo "0=ExoLed(default), 1=ExoSocket, 2=ExoMonsoon"
read input
if [[ -z $input || $input -gt 2 || $input -lt 0 ]]; then
	product_type=0
else
	product_type=$input
fi
product=${product_list[$product_type]}
echo "product: $product"
echo ""

echo "Please input firmware version(default=1): "
read input
if [ -z $input ]; then
	version=1
else
	version=$input
fi
echo "version: $version"
echo ""

boot=new
app=`expr $version % 2`
if [ $app != 1 ]; then
	app=2
fi
spi_speed=40
spi_mode=QIO
spi_size_map=3

echo "boot mode: $boot		(0=boot_v1.1, 1=boot_v1.2+, 2=none)"
echo "app=$app			(0=eagle.flash.bin+eagle.irom0text.bin, 1=user1.bin, 2=user2.bin)"
echo "spi speed: $spi_speed MHz	(0=20MHz, 1=26.7MHz, 2=40MHz, 3=80MHz)"
echo "spi mode: $spi_mode		(0=QIO, 1=QOUT, 2=DIO, 3=DOUT)"
echo "spi_map: $spi_size_map"

touch user/user_main.c

echo ""
echo "start..."
echo ""

make COMPILE=gcc BOOT=$boot APP=$app SPI_SPEED=$spi_speed SPI_MODE=$spi_mode SPI_SIZE_MAP=$spi_size_map PRODUCT_TYPE=$product_type FIRMWARE_VERSION=$version 
mkdir bin/$product
cp -R -f ../bin/upgrade/user$app.2048.new.3.bin bin/$product/${product}_v${version}.bin
