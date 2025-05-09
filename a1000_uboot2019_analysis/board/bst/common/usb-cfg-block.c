// SPDX-License-Identifier: GPL-2.0+
/*
 * usb-cfg-block.c -- USB read cfg from flash or write cfg to flash
 *
 * This file contains proprietary information that is the sole intellectual
 * property of Black Sesame Technologies, Inc. and its affiliates.
 * No portions of this material may be reproduced in any
 * form without the written permission of:
 * Black Sesame Technologies, Inc. and its affiliates
 * 2255 Martin Ave. Suite D
 * Santa Clara, CA 95050
 * Copyright @2016: all right reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <common.h>
#include <linux/usb/ch9.h>
#include <g_dnl.h>

#include <spi.h>
#include <spi_flash.h>

#include <cli.h>
#include <console.h>
#include <flash.h>
#include <malloc.h>
#include <mmc.h>
#include <nand.h>
#include <asm/mach-types.h>

DECLARE_GLOBAL_DATA_PTR;

#if defined(CONFIG_USB_CFG_BLOCK_IS_IN_MMC)
#define USB_CFG_BLOCK_MAX_SIZE 0x80000
#elif defined(CONFIG_USB_CFG_BLOCK_IS_IN_NAND)
#define USB_CFG_BLOCK_MAX_SIZE 0x10000
#elif defined(CONFIG_USB_CFG_BLOCK_IS_IN_NOR)
#define USB_CFG_BLOCK_MAX_SIZE 0x10000
#elif defined(CONFIG_USB_CFG_BLOCK_IS_IN_QSPI)
#define USB_CFG_BLOCK_MAX_SIZE 0x10000
#endif

struct tlv_tag {
	u8 type;
	u8 len;
	u8 value[0];
};
//same with sbl,so sbl can use this 0xcd0000
#define MAX_STRING_SERIAL	32
#define MAX_STRING_MANUFACT	16

struct usb_cfg_block_info {
	int valid_cfgblock;
	int usb_vendor_id;
	int usb_product_id;
	char dnl_serial[MAX_STRING_SERIAL];
	char dnl_manufacturer[MAX_STRING_MANUFACT];
};

#define TAG_VALID	0xee
#define TAG_INVALID	0xff
#define TAG_VID		0x1
#define TAG_PID	0x2
#define TAG_SERIALNUMBER	0x3
#define TAG_MANUFACTURER	0x4

struct usb_cfg_block_info usb_cfg = { 0 };

#ifdef CONFIG_USB_CFG_BLOCK_IS_IN_MMC
static int usb_cfg_block_mmc_storage(u8 *config_block, int write)
{
	struct mmc *mmc;
	int dev = CONFIG_USB_CFG_BLOCK_DEV;
	int offset = CONFIG_USB_CFG_BLOCK_OFFSET;
	uint part = CONFIG_USB_CFG_BLOCK_PART;
	uint blk_start;
	int ret = 0;

	/* Read production parameter config block from eMMC */
	mmc = find_mmc_device(dev);
	if (!mmc) {
		puts("No MMC card found\n");
		ret = -ENODEV;
		goto out;
	}
	if (part != mmc_get_blk_desc(mmc)->hwpart) {
		if (blk_select_hwpart_devnum(IF_TYPE_MMC, dev, part)) {
			puts("MMC partition switch failed\n");
			ret = -ENODEV;
			goto out;
		}
	}
	if (offset < 0)
		offset += mmc->capacity;
	blk_start = ALIGN(offset, mmc->write_bl_len) / mmc->write_bl_len;

	if (!write) {
		/* Careful reads a whole block of 512 bytes into config_block */
		if (blk_dread(mmc_get_blk_desc(mmc), blk_start, 1,
			      (unsigned char *)config_block) != 1) {
			ret = -EIO;
			goto out;
		}
	} else {
		/* Just writing one 512 byte block */
		if (blk_dwrite(mmc_get_blk_desc(mmc), blk_start, 1,
			       (unsigned char *)config_block) != 1) {
			ret = -EIO;
			goto out;
		}
	}

out:
	/* Switch back to regular eMMC user partition */
	blk_select_hwpart_devnum(IF_TYPE_MMC, 0, 0);

	return ret;
}
#endif

#ifdef CONFIG_USB_CFG_BLOCK_IS_IN_NAND
static int read_usb_cfg_block_from_nand(unsigned char *config_block)
{
	size_t size = USB_CFG_BLOCK_MAX_SIZE;
	struct mtd_info *mtd = get_nand_dev_by_index(0);

	if (!mtd)
		return -ENODEV;

	/* Read production parameter config block from NAND page */
	return nand_read_skip_bad(mtd, CONFIG_USB_CFG_BLOCK_OFFSET,
				  &size, NULL, USB_CFG_BLOCK_MAX_SIZE,
				  config_block);
}

static int write_usb_cfg_block_to_nand(unsigned char *config_block)
{
	size_t size = USB_CFG_BLOCK_MAX_SIZE;

	/* Write production parameter config block to NAND page */
	return nand_write_skip_bad(get_nand_dev_by_index(0),
				   CONFIG_USB_CFG_BLOCK_OFFSET,
				   &size, NULL, USB_CFG_BLOCK_MAX_SIZE,
				   config_block, WITH_WR_VERIFY);
}
#endif

#ifdef CONFIG_USB_CFG_BLOCK_IS_IN_NOR
static int read_usb_cfg_block_from_nor(unsigned char *config_block)
{
	/* Read production parameter config block from NOR flash */
	memcpy(config_block, (void *)CONFIG_USB_CFG_BLOCK_OFFSET,
	       USB_CFG_BLOCK_MAX_SIZE);
	return 0;
}

static int write_usb_cfg_block_to_nor(unsigned char *config_block)
{
	/* Write production parameter config block to NOR flash */
	return flash_write((void *)config_block, CONFIG_USB_CFG_BLOCK_OFFSET,
			   USB_CFG_BLOCK_MAX_SIZE);
}
#endif

#ifdef CONFIG_USB_CFG_BLOCK_IS_IN_QSPI
int qspi_write(long dest, const void *src, size_t count)
{
	int ret = -1;
	struct spi_flash *env_flash;

	env_flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
				    CONFIG_SF_DEFAULT_SPEED,
				    CONFIG_SF_DEFAULT_MODE);
	if (!env_flash) {
		puts("SPI probe failed.\n");
		goto done;
	}

	debug("Erasing SPI flash...");
	ret = spi_flash_erase(env_flash, dest, count);
	if (ret) {
		printf("Erasing SPI flash error:%d\n", ret);

		goto done;
	}

	debug("Writing to SPI flash...");

	ret = spi_flash_write(env_flash, dest, count, src);
	if (ret) {
		printf("Writing to SPI flash error:%d\n", ret);

		goto done;
	}

	debug("done\n");
done:
	return ret;
}

int qspi_read(void *dest, long src, size_t count)
{
	int ret = -1;
	struct spi_flash *env_flash;

	env_flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
				    CONFIG_SF_DEFAULT_SPEED,
				    CONFIG_SF_DEFAULT_MODE);
	if (!env_flash) {
		puts("SPI probe failed.\n");
		goto done;
	}
	debug("read to SPI flash...");
	ret = spi_flash_read(env_flash, src, count, dest);
	if (ret) {
		printf("read from SPI flash error:%d\n", ret);

		goto done;
	}

	debug("done\n");
done:
	return ret;
}

static int read_usb_cfg_block_from_qspi(unsigned char *config_block)
{
	/* Read production parameter config block from NOR flash */
	return qspi_read(config_block, CONFIG_USB_CFG_BLOCK_OFFSET,
			 USB_CFG_BLOCK_MAX_SIZE);
}

static int write_usb_cfg_block_to_qspi(unsigned char *config_block)
{
	/* Write production parameter config block to qspi flash */
	return qspi_write(CONFIG_USB_CFG_BLOCK_OFFSET, config_block,
			  USB_CFG_BLOCK_MAX_SIZE);
}
#endif

int read_usb_cfg_block(void)
{
	int ret = 0;
	u8 *config_block = NULL;
	struct tlv_tag *tag;
	size_t size = USB_CFG_BLOCK_MAX_SIZE;
	int offset;
	int exit_flag = 0;

	/* Allocate RAM area for config block */
	config_block = memalign(ARCH_DMA_MINALIGN, size);
	if (!config_block) {
		printf("Not enough malloc space available!\n");
		return -ENOMEM;
	}

	memset(config_block, 0xff, size);

#if defined(CONFIG_USB_CFG_BLOCK_IS_IN_MMC)
	ret = usb_cfg_block_mmc_storage(config_block, 0);
#elif defined(CONFIG_USB_CFG_BLOCK_IS_IN_NAND)
	ret = read_usb_cfg_block_from_nand(config_block);
#elif defined(CONFIG_USB_CFG_BLOCK_IS_IN_NOR)
	ret = read_usb_cfg_block_from_nor(config_block);
#elif defined(CONFIG_USB_CFG_BLOCK_IS_IN_QSPI)
	ret = read_usb_cfg_block_from_qspi(config_block);
#else
	ret = -EINVAL;
#endif
	if (ret)
		goto out;

	/* Expect a valid tag first */
	tag = (struct tlv_tag *)config_block;
	if (tag->type != TAG_VALID) {
		usb_cfg.valid_cfgblock = 0;
		ret = -EINVAL;
		goto out;
	}
	usb_cfg.valid_cfgblock = 1;
	offset = 2;

	while (offset < USB_CFG_BLOCK_MAX_SIZE) {
		tag = (struct tlv_tag *)(config_block + offset);
		offset += 2;
		switch (tag->type) {
		case TAG_INVALID:
			exit_flag = 1;
			break;
		case TAG_VID:
			memcpy(&usb_cfg.usb_vendor_id, config_block + offset,
			       tag->len);
			break;
		case TAG_PID:
			memcpy(&usb_cfg.usb_product_id, config_block + offset,
			       tag->len);
			break;
		case TAG_SERIALNUMBER:
			memcpy(usb_cfg.dnl_serial, config_block + offset,
			       tag->len);
			break;
		case TAG_MANUFACTURER:
			memcpy(usb_cfg.dnl_manufacturer, config_block + offset,
			       tag->len);
			break;
		default:
			break;
		}
		if (exit_flag)
			break;
		offset += tag->len;
	}

out:
	free(config_block);
	return ret;
}

static int get_cfgblock_from_cmd(int argc, char *const argv[])
{
	if (argc < 6)
		return -1;
	usb_cfg.usb_vendor_id = simple_strtoul(argv[2], NULL, 16);
	usb_cfg.usb_product_id = simple_strtoul(argv[3], NULL, 16);
	strncpy(usb_cfg.dnl_serial, argv[4], sizeof(usb_cfg.dnl_serial) - 1);
	strncpy(usb_cfg.dnl_manufacturer, argv[5],
		sizeof(usb_cfg.dnl_manufacturer) - 1);

	return 0;
}

static int write_usb_cfg_block(cmd_tbl_t *cmdtp, int flag, int argc,
			       char *const argv[])
{
	u8 *config_block;
	struct tlv_tag *tag;
	size_t size = USB_CFG_BLOCK_MAX_SIZE;
	int offset = 0;
	int ret = CMD_RET_SUCCESS;
	int err;

	/* Allocate RAM area for config block */
	config_block = memalign(ARCH_DMA_MINALIGN, size);
	if (!config_block) {
		printf("Not enough malloc space available!\n");
		return CMD_RET_FAILURE;
	}

	memset(config_block, 0xff, size);

	err = get_cfgblock_from_cmd(argc, argv);

	if (err) {
		ret = CMD_RET_FAILURE;
		goto out;
	}

	/* Valid Tag */
	tag = (struct tlv_tag *)config_block;
	tag->type = TAG_VALID;
	tag->len = 0;
	offset += 2;

	/* vid Tag */
	tag = (struct tlv_tag *)(config_block + offset);
	tag->type = TAG_VID;
	tag->len = sizeof(usb_cfg.usb_vendor_id);
	offset += 2;
	//tag->value = usb_cfg.usb_vendor_id;
	memcpy(config_block + offset, &usb_cfg.usb_vendor_id, tag->len);
	offset += tag->len;
	/* pid Tag */
	tag = (struct tlv_tag *)(config_block + offset);
	tag->type = TAG_PID;
	tag->len = sizeof(usb_cfg.usb_product_id);
	offset += 2;
	//tag->value = usb_cfg.usb_product_id;
	memcpy(config_block + offset, &usb_cfg.usb_product_id, tag->len);
	offset += tag->len;

	tag = (struct tlv_tag *)(config_block + offset);
	tag->type = TAG_SERIALNUMBER;
	tag->len = strlen(usb_cfg.dnl_serial) + 1;
	offset += 2;
	memcpy(config_block + offset, usb_cfg.dnl_serial, tag->len);
	offset += tag->len;

	tag = (struct tlv_tag *)(config_block + offset);
	tag->type = TAG_MANUFACTURER;
	tag->len = strlen(usb_cfg.dnl_manufacturer) + 1;
	offset += 2;
	memcpy(config_block + offset, usb_cfg.dnl_manufacturer, tag->len);
	offset += tag->len;

	/* invalid Tag */
	tag = (struct tlv_tag *)(config_block + offset);
	tag->type = TAG_INVALID;
	tag->len = 0;
	offset += 2;

#if defined(CONFIG_USB_CFG_BLOCK_IS_IN_MMC)
	err = usb_cfg_block_mmc_storage(config_block, 1);
#elif defined(CONFIG_USB_CFG_BLOCK_IS_IN_NAND)
	err = write_usb_cfg_block_to_nand(config_block);
#elif defined(CONFIG_USB_CFG_BLOCK_IS_IN_NOR)
	err = write_usb_cfg_block_to_nor(config_block);
#elif defined(CONFIG_USB_CFG_BLOCK_IS_IN_QSPI)
	err = write_usb_cfg_block_to_qspi(config_block);
#else
	err = -EINVAL;
#endif
	if (err) {
		printf("Failed to write  config block: %d\n", ret);
		ret = CMD_RET_FAILURE;
		goto out;
	}
	printf("usb config block successfully written\n");

out:
	free(config_block);
	return ret;
}

void usb_cfg_fixup(struct usb_device_descriptor *dev)
{

	read_usb_cfg_block();
	if (usb_cfg.valid_cfgblock) {
		if (usb_cfg.dnl_serial != NULL)
			g_dnl_set_serialnumber(usb_cfg.dnl_serial);
		if (usb_cfg.dnl_manufacturer != NULL)
			g_dnl_set_manufacturer(usb_cfg.dnl_manufacturer);
		if (usb_cfg.usb_vendor_id)
			put_unaligned(usb_cfg.usb_vendor_id, &dev->idVendor);
		if (usb_cfg.usb_product_id)
			put_unaligned(usb_cfg.usb_product_id, &dev->idProduct);
	}
}

static int do_usbcfg_block(cmd_tbl_t *cmdtp, int flag, int argc,
			   char *const argv[])
{
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (!strcmp(argv[1], "write")) {
		return write_usb_cfg_block(cmdtp, flag, argc, argv);
	} else if (!strcmp(argv[1], "read")) {
		ret = read_usb_cfg_block();
		if (ret) {
			printf("Failed to read usb config block: %d\n", ret);
			return CMD_RET_FAILURE;
		}
		printf("cfg %d,vid %x,pid %x,serial %s manufacturer %s\n",
				usb_cfg.valid_cfgblock, usb_cfg.usb_vendor_id,
				usb_cfg.usb_product_id, usb_cfg.dnl_serial,
				usb_cfg.dnl_manufacturer);
		return CMD_RET_SUCCESS;
	}

	return CMD_RET_USAGE;
}

U_BOOT_CMD(usbcfg, 6, 0, do_usbcfg_block,
	   "usb config block handling commands",
	   "usbcfg write vid pid serial manufacturer - write config block\n"
	   "usbcfg read - read usb config block from flash");
