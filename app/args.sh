#!/bin/bash

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
