// SPDX-License-Identifier: GPL-2.0+
/*
 * gadget.c -- USB Gadget fixup
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
#include <dm/ofnode.h>

#ifdef CONFIG_USB_CFG_BLOCK
int usb_cfg_fixup(struct usb_device_descriptor *dev);
#endif

void usb_dtb_fixup(struct usb_device_descriptor *dev)
{
	ofnode node;
	int vid, pid;
	const char *s = NULL;

	node = ofnode_path("/config_manage/usb_gadget");
	if (!ofnode_valid(node)) {
		debug("%s: no config node?\n", __func__);
		return;
	}
	vid = ofnode_read_s32_default(node, "idVendor", 0);
	if (vid)
		put_unaligned(vid, &dev->idVendor);

	pid = ofnode_read_s32_default(node, "idProduct", 0);
	if (pid)
		put_unaligned(pid, &dev->idProduct);

	s = ofnode_read_string(node, "manufacturer");
	if (s)
		g_dnl_set_manufacturer((char *)s);

	s = ofnode_read_string(node, "serialnumber");
	if (s)
		g_dnl_set_serialnumber((char *)s);
}

int g_dnl_bind_fixup(struct usb_device_descriptor *dev, const char *name)
{
	int usb_vendor_id = CONFIG_USB_GADGET_VENDOR_NUM;
	int usb_product_id = CONFIG_USB_GADGET_PRODUCT_NUM;
	char *s = NULL;

	g_dnl_set_manufacturer(CONFIG_USB_GADGET_MANUFACTURER);
#ifdef CONFIG_USB_CFG_BLOCK
	usb_cfg_fixup(dev);
#endif

	usb_dtb_fixup(dev);

	s = env_get("serialnumber");
	if (s)
		g_dnl_set_serialnumber((char *)s);

	s = env_get("manufacturer");
	if (s)
		g_dnl_set_manufacturer((char *)s);

	s = env_get("vid");
	if (s) {
		usb_vendor_id = simple_strtoul((char *)s, NULL, 16);
		put_unaligned(usb_vendor_id, &dev->idVendor);
	}

	s = env_get("pid");
	if (s) {
		usb_product_id = simple_strtoul((char *)s, NULL, 16);
		put_unaligned(usb_product_id, &dev->idProduct);
	}
	return 0;
}
