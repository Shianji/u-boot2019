// SPDX-License-Identifier: GPL-2.0+
#include <common.h>
#include <asm/io.h>
#include <dm/device.h>
#include <dm/pinctrl.h>

DECLARE_GLOBAL_DATA_PTR;

struct bst_pinctrl_priv {
	fdt_addr_t base0; /* pinctrl group 0 reg base */
	fdt_addr_t base1; /* pinctrl group 1 reg base */
};

static int bst_pinctrl_set_state_simple(struct udevice *dev,
					struct udevice *periph)
{
	int entry, count, i;
	uint32_t cell[20];
	uint32_t index, mask, val, group;

	struct bst_pinctrl_priv *priv = dev_get_priv(dev);

	entry = fdtdec_lookup_phandle(gd->fdt_blob, dev_of_offset(periph),
									"pinctrl-0");
	if (entry < 0)
		return -EINVAL;

	count = fdtdec_get_int_array_count(gd->fdt_blob, entry, "group",
						&group, 1);
	if (count != 1)
		return -EINVAL;

	count = fdtdec_get_int_array_count(gd->fdt_blob, entry, "actions,pins",
					   cell, ARRAY_SIZE(cell));
	if (count < 4)
		return -EINVAL;

	for (i = 0; i < count; i += 4) {
		index = cell[i];
		mask = GENMASK(cell[i + 1] + cell[i + 2] - 1, cell[i + 1]);
		val = cell[i + 3] << cell[i + 1];
		switch (group) {
		case 0:
			clrsetbits_le32(priv->base0 + index, mask, val);
			debug("%s: clrsetbits_le32(0x%llx,0x%x,0x%x)\n",
				__func__, priv->base0 + index, mask, val);
			break;
		case 1:
			clrsetbits_le32(priv->base1 + index, mask, val);
			debug("%s: clrsetbits_le32(0x%llx,0x%x,0x%x)\n",
				__func__, priv->base1 + index, mask, val);
			break;
		default:
			pr_err("%s: invalid group ID.\n", __func__);
			break;
		}

	}

	return 0;
}

static const struct pinctrl_ops bst_pinctrl_ops = {
	.set_state_simple = bst_pinctrl_set_state_simple,
};

static int bst_pinctrl_probe(struct udevice *dev)
{
	struct bst_pinctrl_priv *priv = dev_get_priv(dev);

	priv->base0 = fdtdec_get_addr(gd->fdt_blob,
			dev_of_offset(dev), "reggrp0");
	if (priv->base0 == FDT_ADDR_T_NONE)
		return -EINVAL;

	priv->base1 = fdtdec_get_addr(gd->fdt_blob,
			dev_of_offset(dev), "reggrp1");
	if (priv->base1 == FDT_ADDR_T_NONE)
		return -EINVAL;

	debug("%s: base0 is 0x%llx\n", __func__, priv->base0);
	debug("%s: base1 is 0x%llx\n", __func__, priv->base1);

	return 0;
}

static const struct udevice_id bst_pinctrl_match[] = {
	{ .compatible = "bst,a1000-pinctrl" },
	{ }
};

U_BOOT_DRIVER(pinctrl_bst) = {
	.name			= "pinctrl_bst",
	.id				= UCLASS_PINCTRL,
	.of_match		= bst_pinctrl_match,
	.probe			= bst_pinctrl_probe,
	.ops			= &bst_pinctrl_ops,
	.flags			= DM_FLAG_PRE_RELOC,
	.priv_auto_alloc_size	= sizeof(struct bst_pinctrl_priv),
};
