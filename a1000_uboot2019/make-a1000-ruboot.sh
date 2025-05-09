#!/bin/sh

BINOUT=bin_a1000_ru
BUILDOUT=build_a1000_ru

rm -rf ${BINOUT}
mkdir "${BINOUT}" > /dev/null;

CROSS_COMPILETOOL_GNU=aarch64-bst-linux-
CROSS_COMPILETOOL_BST=aarch64-bst-linux-

AR_GUN=${CROSS_COMPILETOOL_GNU}ar
AR_BST=${CROSS_COMPILETOOL_BST}ar
if command -v ${AR_GUN} 1>/dev/null 2>&1; then
    CROSS_COMPILETOOL=${CROSS_COMPILETOOL_GNU}
fi
if command -v ${AR_BST} 1>/dev/null 2>&1; then
    CROSS_COMPILETOOL=${CROSS_COMPILETOOL_BST}
fi

if [ -z "$CROSS_COMPILETOOL" ]; then
    echo 'Failed to find compile tools.'
    exit 1
fi

rm -f ${BUILDOUT}/arch/arm/dts/*.dtb ${BUILDOUT}/arch/arm/dts/.*.cmd \
				${BUILDOUT}/arch/arm/dts/.*.tmp

make O=${BUILDOUT} CROSS_COMPILE=${CROSS_COMPILETOOL} bsta1000_ruboot_defconfig
make O=${BUILDOUT} CROSS_COMPILE=${CROSS_COMPILETOOL} -j$(nproc)

cp ${BUILDOUT}/u-boot.bin ${BINOUT}/
cp atf/bst-bl31-el2.bin ${BINOUT}/atf.bin
cp atf/mkRuboot ${BINOUT}/

cd ${BINOUT}

chmod 777 mkRuboot
./mkRuboot u-boot.bin atf.bin
rm mkRuboot u-boot.bin atf.bin
mv ATF-u-boot.bin RUBOOT-A1000-512K.bin
cd -
