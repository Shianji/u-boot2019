#!/bin/sh

BINOUT=bin_a1000
BUILDOUT=build_a1000

rm -rf ${BINOUT}
mkdir "${BINOUT}" > /dev/null;


CROSS_COMPILETOOL_GNU=aarch64-bst-linux-
CROSS_COMPILETOOL_BST=aarch64-bst-linux-

AR_GUN=${CROSS_COMPILETOOL_GNU}ar
AR_BST=${CROSS_COMPILETOOL_BST}ar
if command -v ${AR_GUN} 1>/dev/null 2>&1; then
    CROSS_COMPILETOOL=aarch64-bst-linux-
fi
if command -v ${AR_BST} 1>/dev/null 2>&1; then
    CROSS_COMPILETOOL=aarch64-bst-linux-
fi

if [ -z "$CROSS_COMPILETOOL" ]; then
    echo 'Failed to find compile tools.'
    exit 1
fi


rm -f ${BUILDOUT}/arch/arm/dts/*.dtb ${BUILDOUT}/arch/arm/dts/.*.cmd \
				${BUILDOUT}/arch/arm/dts/.*.tmp

make O=${BUILDOUT} CROSS_COMPILE=${CROSS_COMPILETOOL} bsta1000_defconfig
make O=${BUILDOUT} CROSS_COMPILE=${CROSS_COMPILETOOL} -j$(nproc)

cp ${BUILDOUT}/u-boot.bin ${BINOUT}/uboot-a1000.bin
