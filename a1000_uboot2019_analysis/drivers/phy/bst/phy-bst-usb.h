/* SPDX-License-Identifier: GPL-2.0
 * phy driver for BST usb
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

#ifndef _PHY_BST_USB_H
#define _PHY_BST_USB_H
#include <common.h>
#include <dm.h>
#include <log.h>
#include <dm/device.h>
#include <generic-phy.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <reset.h>

#define USB3PHY_REG_1           0x14
#define FSEL_POS            0
#define FSEL_MSK            (0x3f<<FSEL_POS)
#define MPLLMULTIPLIER_POS  6
#define MPLLMULTIPLIER_MSK  (0x7f<<MPLLMULTIPLIER_POS)
#define SSCREFCLKSEL_POS    23
#define SSCREFCLKSEL_MSK    (0x1ff<<SSCREFCLKSEL_POS)

#define USB3PHY_REG_2           0x18
#define SSCEN               BIT(0)
#define REFUSEPAD           BIT(5)
#define REFSSPEN            BIT(6)

#define USB3PHY_REG_4           0x20
#define REFCLKDIV2          BIT(2)
#define REFCLKSEL_POS       0
#define REFCLKSEL_MSK       (0x3<<REFCLKSEL_POS)

#define USB3PHY_REG_5           0x60

#define USB3PHY_LOCAL_RST       0x68
#define PHYSWRSTN           BIT(0)
#define CTRLSWRSTN          BIT(1)

#define USB2PHY_PWR_CTRL        0x10
#define COMMONONN           BIT(0)
#define PHYRESET            BIT(1)

enum usb_mode_enum {
	USB_MODE_UNKNOWN,
	USB_MODE_USB2,
	USB_MODE_USB3
};

struct bst_usb {
	//struct usb_phy                phy;
	void __iomem *phy_base;
	struct udevice *dev;
	enum usb_mode_enum usb_mode;
	int external_clk;
	struct reset_ctl_bulk reset;
};

#endif
