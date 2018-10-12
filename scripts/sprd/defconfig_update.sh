#!/bin/bash

SRC_PATH="`dirname $0`/../.."

cd $SRC_PATH

DEFCONF_ARM64=`find arch/arm64/configs/ -name sprd_\*_defconfig -printf "%f\n"`

export ARCH=arm64
for def in $DEFCONF_ARM64;do
	if [ -f arch/arm64/configs/$def ]; then
		if  make $def ; then
			if ! diff .config arch/arm64/configs/$def; then
				echo "ERROR: arm64 defconfig $def miss order"
				if [ "$1" != "dry" ]; then
					cp -v .config arch/arm64/configs/$def
					echo "arm64 defconfig $def updated"
				fi
			else
				echo "arm64 defconfig $def equals"
			fi
		else
			echo "ERROR: make defconfig $def failed"
			exit 1
		fi
	fi
done
