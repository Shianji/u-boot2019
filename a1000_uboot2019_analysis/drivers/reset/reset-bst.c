// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020 Bst <yi.zhang@bst.ai>
 *
 * Derived from driver/reset/reset-bst.c
 */
#include <common.h>
#include <dm.h>
#include <dm/of_access.h>
#include <reset-uclass.h>
#include <linux/bitops.h>
#include <linux/io.h>
#include <linux/sizes.h>

struct bst_reset_data {
	void __iomem *membase;
};

static int bst_reset_assert(struct reset_ctl *reset_ctl)
{
	struct bst_reset_data *data = dev_get_priv(reset_ctl->dev);

	clrbits_le32(data->membase, BIT(reset_ctl->id));
	mdelay(20);

	return 0;
}

static int bst_reset_deassert(struct reset_ctl *reset_ctl)
{
	struct bst_reset_data *data = dev_get_priv(reset_ctl->dev);

	setbits_le32(data->membase, BIT(reset_ctl->id));
	mdelay(20);

	return 0;
}

static int bst_reset_request(struct reset_ctl *reset_ctl)
{
	debug("%s(reset_ctl=%p) (dev=%p, id=%lu)\n", __func__,
	reset_ctl, reset_ctl->dev, reset_ctl->id);

	return 0;
}

static int bst_reset_free(struct reset_ctl *reset_ctl)
{
	debug("%s(reset_ctl=%p) (dev=%p, id=%lu)\n", __func__, reset_ctl,
	reset_ctl->dev, reset_ctl->id);

return 0;
}

static const struct reset_ops bst_reset_ops = {
	.request = bst_reset_request,
	.free = bst_reset_free,
	.rst_assert = bst_reset_assert,
	.rst_deassert = bst_reset_deassert,
};

static int bst_reset_probe(struct udevice *dev)
{
	struct bst_reset_data *data = dev_get_priv(dev);

	data->membase = devfdt_get_addr_ptr(dev);

return 0;
}

static const struct udevice_id bst_reset_match[] = {
	{ .compatible = "bst,a1000-reset" },
	{ /* sentinel */ },
};

U_BOOT_DRIVER(bst_reset) = {
	.name = "bst-reset",
	.id = UCLASS_RESET,
	.of_match = bst_reset_match,
	.probe = bst_reset_probe,
	.priv_auto_alloc_size = sizeof(struct bst_reset_data),
	.ops = &bst_reset_ops,
};
