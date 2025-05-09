// SPDX-License-Identifier: GPL-2.0
/*
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


#include "phy-bst-usb.h"
#define DEBUG 1
static const char *const usb_modes[] = {
	[USB_MODE_UNKNOWN] = "",
	[USB_MODE_USB2] = "usb20",
	[USB_MODE_USB3] = "usb30",
};
static int match_string(const char * const *array, size_t n, const char *string)
{
	int index;
	const char *item;

	for (index = 0; index < n; index++) {
		item = array[index];
		if (!item)
			break;
		if (!strcmp(item, string))
			return index;
	}

	return -EINVAL;
}
static enum usb_mode_enum usb_get_usb_mode_from_string(const char *str)
{
	int ret;

	ret = match_string(usb_modes, ARRAY_SIZE(usb_modes), str);
	return (ret < 0) ? USB_MODE_UNKNOWN : ret;
}

enum usb_mode_enum usb_get_usb_mode(struct udevice *dev)
{
	const char *usb_mode;

	usb_mode = dev_read_string(dev, "usb_mode");
	if (usb_mode == NULL)
		return USB_MODE_UNKNOWN;

	return usb_get_usb_mode_from_string(usb_mode);
}

static int bst_usb_phy_power(struct bst_usb *phy, int on)
{
	return 0;
}

static int bst_usb_power_off(struct phy *x)
{
	struct udevice *dev = x->dev;
	struct bst_usb *phy = dev_get_priv(dev);

	return bst_usb_phy_power(phy, false);
}

static int bst_usb_power_on(struct phy *x)
{
	struct udevice *dev = x->dev;
	struct bst_usb *phy = dev_get_priv(dev);

	return bst_usb_phy_power(phy, true);
}

void bst_usb3_phy_init(struct bst_usb *phy)
{
	void __iomem *phy_base = NULL;
	u32 reg = 0;

	phy_base = phy->phy_base;
	//ssc ssp
	reg = readl(phy_base + USB3PHY_REG_2);
	reg |= SSCEN;
	reg |= REFSSPEN;
	writel(reg, phy_base + USB3PHY_REG_2);
	if (phy->external_clk) {
		/* USB3 PLL config */
		reg = readl(phy_base + USB3PHY_REG_1);
		reg &= ~FSEL_MSK;
		reg |= (0x27 << FSEL_POS);
		reg &= ~MPLLMULTIPLIER_MSK;
		reg &= ~SSCREFCLKSEL_MSK;
		writel(reg, phy_base + USB3PHY_REG_1);

		reg = readl(phy_base + USB3PHY_REG_4);
		reg &= (~(REFCLKDIV2 | REFCLKSEL_MSK));
		writel(reg, phy_base + USB3PHY_REG_4);

		reg = readl(phy_base + USB3PHY_REG_2);
		reg |= REFUSEPAD;
		writel(reg, phy_base + USB3PHY_REG_2);
	} else {
		reg = readl(phy_base + USB3PHY_REG_2);
		reg &= ~REFUSEPAD;
		writel(reg, phy_base + USB3PHY_REG_2);

		reg = readl(phy_base + USB3PHY_REG_5);
		reg |= (BIT(11) | BIT(14));
		writel(reg, phy_base + USB3PHY_REG_5);
	}
	//reset
	reg = readl(phy_base + USB3PHY_LOCAL_RST);
	reg |= (PHYSWRSTN | CTRLSWRSTN);
	writel(reg, phy_base + USB3PHY_LOCAL_RST);
}

void bst_usb2_phy_init(struct bst_usb *phy)
{
	writel((COMMONONN | PHYRESET), phy->phy_base + USB2PHY_PWR_CTRL);
	writel(COMMONONN, phy->phy_base + USB2PHY_PWR_CTRL);
}

static int bst_usb_init(struct phy *x)
{
	struct udevice *dev = x->dev;
	struct bst_usb *phy = dev_get_priv(dev);
	int ret = 0;

	if (phy) {
		ret = reset_deassert_bulk(&phy->reset);
		if (ret) {
			return ret;
		}
		if (phy->usb_mode == USB_MODE_USB2)
			bst_usb2_phy_init(phy);
		else
			bst_usb3_phy_init(phy);
	}
	return ret;
}

static int bst_usb_exit(struct phy *x)
{
	return 0;
}

//ret 0 internal,others external
static int usb_get_usb_pll_type(struct udevice *dev)
{
	const char *pll_type = NULL;

	pll_type = dev_read_string(dev, "pll_type");
	if (pll_type && strncmp(pll_type, "internal", 5))
		return 1;
	return 0;
}

static int bst_usb_probe(struct udevice *dev)
{
	struct bst_usb *phy;
	enum usb_mode_enum usb_mode;
	int ret = 0;

	phy = dev_get_priv(dev);
	usb_mode = usb_get_usb_mode(dev);
	if (usb_mode == USB_MODE_UNKNOWN) {
		pr_err("missing usb mode settting.\n");
		return -ENODEV;
	}
	ret = reset_get_bulk(dev, &phy->reset);
	if (ret != 0 && ret != -ENOTSUPP && ret != -ENOENT) {
		pr_err("get reset error %d.\n", ret);
		return ret;
	}

	phy->external_clk = usb_get_usb_pll_type(dev);
	phy->usb_mode = usb_mode;
	phy->dev = dev;
	phy->phy_base = (void __iomem *)dev_read_addr(dev);
	if (IS_ERR(phy->phy_base)) {
		pr_err("devm_ioremap_resource err\n");
		return PTR_ERR(phy->phy_base);
	}
	return 0;
}

static int bst_usb_remove(struct udevice *dev)
{
	struct bst_usb *phy = dev_get_priv(dev);

	if (phy)
		reset_release_bulk(&phy->reset);
	return 0;
}

static const struct udevice_id bst_usb_ids[] = {
	{ .compatible = "bst,dwc-usb-phy" },
	{ }
};

static struct phy_ops bst_usb_ops = {
	.init = bst_usb_init,
	.power_on = bst_usb_power_on,
	.power_off = bst_usb_power_off,
	.exit = bst_usb_exit,
};

U_BOOT_DRIVER(bst_usb_phy) = {
	.name	= "bst_usb_phy",
	.id	= UCLASS_PHY,
	.of_match = bst_usb_ids,
	.ops = &bst_usb_ops,
	.probe = bst_usb_probe,
	.remove	= bst_usb_remove,
	.priv_auto_alloc_size	= sizeof(struct bst_usb),
};

