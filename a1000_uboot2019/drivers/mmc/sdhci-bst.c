// SPDX-License-Identifier: GPL-2.0
/*
 * Driver for Synopsys DesignWare Cores Mobile Storage Host Controller
 *
 * Copyright (C) 2018 Synaptics Incorporated
 *
 * Author: Jisheng Zhang <jszhang@kernel.org>
 */


#include <common.h>
#include <dm.h>
#include <iotrace.h>
#include <linux/bitfield.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/sizes.h>
#include <linux/libfdt.h>
#include <mmc.h>
#include <sdhci.h>
#include <reset.h>

//bit0 for sd0/emmc0  bit1 for sd1/emmc1  (0:sd  1:emmc)
#define REG_SD_EMMC_SEL					0x33000064
#define BST_SDMMC_VER_ID				0x3138302A
#define SDHCI_VENDOR_PTR_R				0xE8
#define SYS_CTRL_SDEMMC_DIV_CTRL		0x3300003C
#define SYS_CTRL_SDEMMC_CTRL_EN_CLR		0x33000068

/* Synopsys vendor specific registers */
#define reg_offset_addr_vendor	(sdhci_readw(host, SDHCI_VENDOR_PTR_R))
#define SDHC_MHSC_VER_ID_R		(reg_offset_addr_vendor)
#define SDHC_MHSC_VER_TPYE_R	(reg_offset_addr_vendor+0X4)
#define SDHC_MHSC_CTRL_R		(reg_offset_addr_vendor+0X8)
#define SDHC_MBIU_CTRL_R		(reg_offset_addr_vendor+0X10)
#define SDHC_EMMC_CTRL_R		(reg_offset_addr_vendor+0X2C)
#define SDHC_BOOT_CTRL_R		(reg_offset_addr_vendor+0X2E)
#define SDHC_GP_IN_R			(reg_offset_addr_vendor+0X30)
#define SDHC_GP_OUT_R			(reg_offset_addr_vendor+0X34)
#define SDHC_AT_CTRL_R			(reg_offset_addr_vendor+0X40)
#define SDHC_AT_STAT_R			(reg_offset_addr_vendor+0X44)

#define MBIU_CTRL  0x510
#define BURST_INCR16_EN  BIT(3)
#define BURST_INCR8_EN  BIT(2)
#define BURST_INCR4_EN  BIT(1)
#define BURST_EN (BURST_INCR16_EN|BURST_INCR8_EN|BURST_INCR4_EN)

struct bst_sdhci_plat {
	struct mmc_config cfg;
	struct mmc mmc;
	unsigned int f_max;
	unsigned int f_min;
};


struct bst_sdhci_priv {
	struct sdhci_host *host;
	struct reset_ctl_bulk	resets;
	u8 no_1p8;
};

void bst_card_clk_enable(struct sdhci_host *host, u32 enable)
{
	u16 clock_ctrl;

	clock_ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	if (enable)
		sdhci_writew(host,
			clock_ctrl|SDHCI_CLOCK_CARD_EN,
			SDHCI_CLOCK_CONTROL);
	else
		sdhci_writew(host,
			clock_ctrl&(~SDHCI_CLOCK_CARD_EN),
			SDHCI_CLOCK_CONTROL);
}

void bst_pll_clk_enable(struct sdhci_host *host, u32 enable)
{
	u16 clock_ctrl;

	clock_ctrl = sdhci_readw(host, SDHCI_CLOCK_CONTROL);
	if (enable)
		sdhci_writew(host,
			clock_ctrl|SDHCI_CLOCK_PLL_EN,
			SDHCI_CLOCK_CONTROL);
	else
		sdhci_writew(host,
			clock_ctrl&(~SDHCI_CLOCK_PLL_EN),
			SDHCI_CLOCK_CONTROL);
}

void bst_mmc_clock_freq_change(struct sdhci_host *host, u32 i_div)
{
	u32 div;
	u32 tmp;

	div = (i_div<<1);
	//Execute SD Clock Stop Sequence
	bst_card_clk_enable(host, 0);
	//Set CLK_CTRL_R.PLL_ENABLE to 0
	bst_pll_clk_enable(host, 0);
	tmp = readl((void *)SYS_CTRL_SDEMMC_DIV_CTRL);
	if (host->ioaddr == MMC0_BASE) {
		writel(tmp&(~(0x3ff<<10))|((div&0x3ff)<<10),
				(void *)SYS_CTRL_SDEMMC_DIV_CTRL);
		writel(readl((void *)SYS_CTRL_SDEMMC_CTRL_EN_CLR)|BIT(4),
				(void *)SYS_CTRL_SDEMMC_CTRL_EN_CLR);
	} else {
		writel(tmp&(~(0x3ff))|(div&0x3ff),
				(void *)SYS_CTRL_SDEMMC_DIV_CTRL);
		writel(readl((void *)SYS_CTRL_SDEMMC_CTRL_EN_CLR)|BIT(8),
				(void *)SYS_CTRL_SDEMMC_CTRL_EN_CLR);
	}
	sdhci_writew(host, (div&0xff)<<8, SDHCI_CLOCK_CONTROL);
	//Check HOST_CTRL2_R.PRESET_VAL_ENABLE //contrl by dut
	//Set CLK_CTRL_R.PLL_ENABLE to 1
	bst_pll_clk_enable(host, 1);
	bst_card_clk_enable(host, 1);
	sdhci_writew(host,
		sdhci_readw(host, SDHCI_CLOCK_CONTROL)|SDHCI_CLOCK_INT_EN,
		SDHCI_CLOCK_CONTROL);
}

static void bst_set_clock(struct sdhci_host *host, u32 div)
{
	/* ToDo : Use the Clock Framework */
	bst_mmc_clock_freq_change(host, div);
}

/* Platform specific function for post set_ios configuration */
static void bst_sdhci_set_ios_post(struct sdhci_host *host)
{

}

static void bst_sdhci_set_control_reg(struct sdhci_host *host)
{
	sdhci_writew(host, sdhci_readw(host,
			SDHCI_HOST_CONTROL)|SDHCI_CTRL_HISPD,
			SDHCI_HOST_CONTROL);
	if (host->ioaddr == MMC0_BASE)
		sdhci_writew(host, SDHCI_CTRL_VDD_180|SDHCI_CTRL_UHS_SDR25,
				SDHCI_HOST_CONTROL2);
	sdhci_writew(host,
			(sdhci_readw(host, MBIU_CTRL)&(~0xf))|BURST_EN,
			MBIU_CTRL);
}

static int bst_sdhci_reset(struct udevice *dev)
{
	struct bst_sdhci_priv *priv = dev_get_priv(dev);
	int ret;

	ret = reset_get_bulk(dev, &priv->resets);
	if (ret) {
		/*
		 * Return 0 if error due to !CONFIG_DM_RESET and reset
		 * DT property is not present.
		 */
		if (ret == -ENOENT || ret == -ENOTSUPP)
			return 0;

		dev_warn(bus, "Can't get reset: %d\n", ret);
		return ret;
	}

	ret = reset_deassert_bulk(&priv->resets);
	if (ret) {
		reset_release_bulk(&priv->resets);
		dev_err(bus, "Failed to reset: %d\n", ret);
		return ret;
	}

	return 0;
}


static const struct sdhci_ops bst_sdhci_ops = {

	.set_clock = &bst_set_clock,
	.set_ios_post = &bst_sdhci_set_ios_post,
	.set_control_reg = &bst_sdhci_set_control_reg,
};

#ifdef CONFIG_BLK
static int bst_sdhci_bind(struct udevice *dev)
{
	struct bst_sdhci_plat *plat = dev_get_platdata(dev);

	return sdhci_bind(dev, &plat->mmc, &plat->cfg);
}
#else
static int sdhci_bst_bind(struct udevice *dev)
{
	return 0;
}
#endif

#if CONFIG_IS_ENABLED(DM_MMC)
static int bst_sdhci_probe(struct udevice *dev)
{
	//    DECLARE_GLOBAL_DATA_PTR;
	struct mmc_uclass_priv *upriv = dev_get_uclass_priv(dev);
	struct bst_sdhci_plat *plat = dev_get_platdata(dev);
	struct bst_sdhci_priv *priv = dev_get_priv(dev);
	struct sdhci_host *host = priv->host;
	u32 bus_width;
	int ret;

	debug("sdhci_bst_probe\n");

	/* sdemmc block soft reset */
	ret = bst_sdhci_reset(dev);
	if (ret)
		return ret;

	/* set timer div and io strength */
	writel(0xc8c8c8c8, 0x3300005C);//timer 25m
	writel(0x00, 0x33000060);//  tx
	writel(0xFF0, 0x33000068);//enable sdemmc div
	writel(readl(0x330010A0)|0x06060606, 0x330010A0);//  io strength
	writel(readl(0x330010A4)|0x06060606, 0x330010A4);//  io strength
	writel(readl(0x330010A8)|0x06060606, 0x330010A8);//  io strength
	writel(readl(0x330010AC)|0x06060606, 0x330010AC);//  io strength
	writel(readl(0x330010B0)|0x03030303, 0x330010B0);//  io strength
	writel(readl(0x330010B4)|0x03030303, 0x330010B4);//  io strength
	writel(readl(0x330010B8)|0x03030303, 0x330010B8);//  io strength

	if (sdhci_readl(host, SDHC_MHSC_VER_ID_R) != BST_SDMMC_VER_ID) {
		dev_err(dev, "BST_SDMMC_VER_ID do not match.\n");
		return -EINVAL;
	}

	host->max_clk = plat->f_max;
	host->quirks = SDHCI_QUIRK_WAIT_SEND_CMD|SDHCI_QUIRK_32BIT_DMA_ADDR;
	if (priv->no_1p8)
		host->quirks |= SDHCI_QUIRK_NO_1_8_V;
	/* do not switch voltage */
	if (host->ioaddr == MMC0_BASE)
		host->quirks |= SDHCI_QUIRK_BROKEN_VOLTAGE;

	host->host_caps = SDHCI_CAN_DO_SDMA | SDHCI_CAN_VDD_180;
	if (host->bus_width == 8)
		host->host_caps |= SDHCI_CAN_DO_8BIT;
	if (host->ioaddr == MMC1_BASE)
		host->host_caps |= SDHCI_CAN_VDD_330|SDHCI_CAN_VDD_300;

	ret = mmc_of_parse(dev, &plat->cfg);
	if (ret) {
		debug("mmc_of_parse\n");
		return ret;
	}

	ret = sdhci_setup_cfg(&plat->cfg, host, plat->f_max, plat->f_min);
	if (ret) {
		debug("sdhci_setup_cfg\n");
		return ret;
	}
	host->mmc = &plat->mmc;
	host->mmc->priv = host;
	host->mmc->dev = dev;
	upriv->mmc = host->mmc;

	return sdhci_probe(dev);
}

static int bst_sdhci_ofdata_to_platdata(struct udevice *dev)
{
	struct bst_sdhci_plat *plat = dev_get_platdata(dev);
	struct bst_sdhci_priv *priv = dev_get_priv(dev);

	priv->host = calloc(1, sizeof(struct sdhci_host));
	if (!priv->host)
		return -1;

	priv->host->name = dev->name;
	priv->host->ops = &bst_sdhci_ops;
	priv->host->ioaddr = (void *)dev_read_addr(dev);
	if (IS_ERR(priv->host->ioaddr))
		return PTR_ERR(priv->host->ioaddr);
	priv->host->bus_width = dev_read_u32_default(dev, "bus-width", 4);
	priv->no_1p8 = dev_read_bool(dev, "no-1-8-v");

	plat->f_max = dev_read_u32_default(dev, "max-frequency", 0);
	plat->f_min = dev_read_u32_default(dev, "min-frequency", 0);

	return 0;
}

static const struct udevice_id bst_sdhci_match[] = {
	{ .compatible = "bst,sdhci" },
};

U_BOOT_DRIVER(sdhci_bst) = {
	.name = "sdhci-bst",
	.id = UCLASS_MMC,
	.of_match = bst_sdhci_match,
	.ops = &sdhci_ops,
	.priv_auto_alloc_size = sizeof(struct bst_sdhci_priv),
	.platdata_auto_alloc_size = sizeof(struct bst_sdhci_plat),
	.bind = bst_sdhci_bind,
	.probe = bst_sdhci_probe,
	.ofdata_to_platdata = bst_sdhci_ofdata_to_platdata,
};
#endif

static void sdhci_bst_print_vendor(struct sdhci_host *host)
{
	printf("============ SDHCI VENDOR REGISTER DUMP ===========\n");

	printf("VER_ID:  0x%08x | VER_TPYE:  0x%08x\n",
		sdhci_readl(host, SDHC_MHSC_VER_ID_R),
		sdhci_readl(host, SDHC_MHSC_VER_TPYE_R));
}

static int init_bst_mmc_core(struct sdhci_host *host)
{
	unsigned int mask;
	unsigned int timeout;

	if ((sdhci_readb(host, SDHCI_SOFTWARE_RESET) & SDHCI_RESET_ALL)) {
		printf("%s: sd host controller reset error\n", __func__);
		return -EBUSY;
	}
	return 0;
}

int bst_sdhci_init(u32 base, u32 min_clk, u32 quirks)
{
	int ret = 0;
	u32 max_clk;
	void *reg_base;
	struct sdhci_host *host = NULL;

	host = (struct sdhci_host *)malloc(sizeof(struct sdhci_host));
	if (!host) {
		printf("%s: sdhci host malloc fail!\n", __func__);
		return -ENOMEM;
	}
	reg_base = (void *)base;
	max_clk = 200000000ul;
	if (reg_base == 0x30400000) {
		host->name = "bst-sdhci0";
		host->host_caps = SDHCI_CAN_DO_SDMA | SDHCI_CAN_VDD_180|
					SDHCI_CAN_DO_8BIT;
		host->host_caps |= MMC_MODE_HS | MMC_MODE_4BIT | MMC_MODE_8BIT;
	} else if (reg_base == 0x30500000) {
		host->name = "bst-sdhci1";
		host->host_caps = SDHCI_CAN_DO_SDMA | SDHCI_CAN_VDD_330|
					SDHCI_CAN_VDD_300|SDHCI_CAN_VDD_180;
		host->host_caps |= MMC_MODE_HS | MMC_MODE_4BIT;
	}
	host->ioaddr = reg_base;
	host->quirks = quirks;
	host->max_clk = max_clk;
	host->ops = &bst_sdhci_ops;

	if (init_bst_mmc_core(host)) {
		free(host);
		return -EINVAL;
	}

#ifdef BLK

#else
	add_sdhci(host, 0, min_clk);
#endif
	return ret;
}
