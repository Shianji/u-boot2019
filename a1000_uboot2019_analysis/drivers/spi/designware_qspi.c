// SPDX-License-Identifier: GPL-2.0
/*
 * Designware master SPI core controller driver
 *
 * Copyright (C) 2014 Stefan Roese <sr@denx.de>
 *
 * Very loosely based on the Linux driver:
 * drivers/spi/spi-dw.c, which is:
 * Copyright (c) 2009, Intel Corporation.
 */

#include <common.h>
#include <asm-generic/gpio.h>
#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <malloc.h>
#include <spi.h>
#include <fdtdec.h>
#include <reset.h>
#include <linux/compat.h>
#include <linux/iopoll.h>
#include <watchdog.h>
#include <asm/io.h>
#include <watchdog.h>


DECLARE_GLOBAL_DATA_PTR;

/* Register offsets */
#define DW_QSPI_CTRL0			0x00
#define DW_QSPI_CTRL1			0x04
#define DW_QSPI_SSIENR			0x08
#define DW_QSPI_MWCR			0x0c
#define DW_QSPI_SER			0x10
#define DW_QSPI_BAUDR			0x14
#define DW_QSPI_TXFLTR			0x18
#define DW_QSPI_RXFLTR			0x1c
#define DW_QSPI_TXFLR			0x20
#define DW_QSPI_RXFLR			0x24
#define DW_QSPI_SR			0x28
#define DW_QSPI_IMR			0x2c
#define DW_QSPI_ISR			0x30
#define DW_QSPI_RISR			0x34
#define DW_QSPI_TXOICR			0x38
#define DW_QSPI_RXOICR			0x3c
#define DW_QSPI_RXUICR			0x40
#define DW_QSPI_MSTICR			0x44
#define DW_QSPI_ICR			0x48
#define DW_QSPI_DMACR			0x4c
#define DW_QSPI_DMATDLR			0x50
#define DW_QSPI_DMARDLR			0x54
#define DW_QSPI_IDR			0x58
#define DW_QSPI_VERSION			0x5c
#define DW_QSPI_DR			0x60

/* Bit fields in CTRLR0 */
#define SPI_DFS_OFFSET			0

#define SPI_FRF_OFFSET			6
#define SPI_FRF_SPI			0x0
#define SPI_FRF_SSP			0x1
#define SPI_FRF_MICROWIRE		0x2
#define SPI_FRF_RESV			0x3

#define SPI_MODE_OFFSET			8
#define SPI_SCPH_OFFSET			8
#define SPI_SCOL_OFFSET			9

#define SPI_TMOD_OFFSET			10
#define SPI_TMOD_MASK			(0x3 << SPI_TMOD_OFFSET)
#define	SPI_TMOD_TR			0x0		/* xmit & recv */
#define SPI_TMOD_TO			0x1		/* xmit only */
#define SPI_TMOD_RO			0x2		/* recv only */
#define SPI_TMOD_EPROMREAD		0x3		/* eeprom read mode */

#define SPI_SLVOE_OFFSET		10
#define SPI_SRL_OFFSET			11
#define SPI_CFS_OFFSET			12

/* Bit fields in SR, 7 bits */
#define SR_MASK				GENMASK(6, 0)	/* cover 7 bits */
#define SR_BUSY				BIT(0)
#define SR_TF_NOT_FULL			BIT(1)
#define SR_TF_EMPT			BIT(2)
#define SR_RF_NOT_EMPT			BIT(3)
#define SR_RF_FULL			BIT(4)
#define SR_TX_ERR			BIT(5)
#define SR_DCOL				BIT(6)

#define RX_TIMEOUT			1000		/* timeout in ms */

struct dw_qspi_platdata {
	s32 frequency;		/* Default clock frequency, -1 for none */
	void __iomem *regs;
};

struct dw_qspi_priv {
	void __iomem *regs;
	unsigned int freq;		/* Default frequency */
	unsigned int mode;
	struct clk clk;
	unsigned long bus_clk_rate;

	struct gpio_desc cs_gpio;	/* External chip-select gpio */

	int bits_per_word;
	u8 cs;			/* chip select pin */
	u8 tmode;		/* TR/TO/RO/EEPROM */
	u8 type;		/* SPI/SSP/MicroWire */
	int len;

	u32 fifo_len;		/* depth of the FIFO buffer */
	void *tx;
	void *tx_end;
	void *rx;
	void *rx_end;

	struct reset_ctl_bulk	resets;
};

static inline u32 dw_read(struct dw_qspi_priv *priv, u32 offset)
{
	return __raw_readl(priv->regs + offset);
}

static inline void dw_write(struct dw_qspi_priv *priv, u32 offset, u32 val)
{
	__raw_writel(val, priv->regs + offset);
}

static void mem_dump(char *buff, int len)
{
	int i;

	for (i = 0 ; i < len; i++) {
		if (!(i&0xf))
			printf("\n\r[%p]", buff+i);
		printf(" %02x", *(buff+i));
	}
	printf("\n\r");
}
static int request_gpio_cs(struct udevice *bus)
{
#if defined(CONFIG_DM_GPIO)
	struct dw_qspi_priv *priv = dev_get_priv(bus);
	int ret;

	/* External chip select gpio line is optional */
	ret = gpio_request_by_name(bus, "cs-gpio", 0, &priv->cs_gpio, 0);
	if (ret == -ENOENT)
		return 0;

	if (ret < 0) {
		printf("Error: %d: Can't get %s gpio!\n", ret, bus->name);
		return ret;
	}

	if (dm_gpio_is_valid(&priv->cs_gpio)) {
		dm_gpio_set_dir_flags(&priv->cs_gpio,
				      GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);
	}

	debug("%s: used external gpio for CS management\n", __func__);
#endif
	return 0;
}

static int dw_qspi_ofdata_to_platdata(struct udevice *bus)
{
	struct dw_qspi_platdata *plat = bus->platdata;
	const void *blob = gd->fdt_blob;
	int node = dev_of_offset(bus);

	plat->regs = (struct dw_spi *)devfdt_get_addr(bus);

	/* Use 500KHz as a suitable default */
	plat->frequency = fdtdec_get_int(blob, node, "spi-max-frequency",
					500000);
	debug("%s: regs=%p max-frequency=%d\n", __func__, plat->regs,
	      plat->frequency);

	return request_gpio_cs(bus);
}

static inline void spi_enable_chip(struct dw_qspi_priv *priv, int enable)
{
	dw_write(priv, DW_QSPI_SSIENR, (enable ? 1 : 0));
}

/* Restart the controller, disable all interrupts, clean rx fifo */
static void qspi_hw_init(struct dw_qspi_priv *priv)
{
	spi_enable_chip(priv, 0);
	dw_write(priv, DW_QSPI_IMR, 0xff);
	spi_enable_chip(priv, 1);

	/*
	 * Try to detect the FIFO depth if not set by interface driver,
	 * the depth could be from 2 to 256 from HW spec
	 */
	if (!priv->fifo_len) {
		u32 fifo;

		for (fifo = 1; fifo < 8; fifo++) {
			dw_write(priv, DW_QSPI_TXFLTR, fifo);
			if (fifo != dw_read(priv, DW_QSPI_TXFLTR))
				break;
		}

		priv->fifo_len = (fifo == 1) ? 0 : fifo;
		dw_write(priv, DW_QSPI_TXFLTR, 0);
	}
	debug("%s: fifo_len=%d\n", __func__, priv->fifo_len);
}

/*
 * We define dw_qspi_get_clk function as 'weak' as some targets
 * (like SOCFPGA_GEN5 and SOCFPGA_ARRIA10) don't use standard clock API
 * and implement dw_qspi_get_clk their own way in their clock manager.
 */
__weak int dw_qspi_get_clk(struct udevice *bus, ulong *rate)
{
#if 1
	*rate = 25000000UL;
	debug("%s: get spi controller clk via device tree: %lu Hz\n",
	      __func__, *rate);
	return 0;
#else
	struct dw_qspi_priv *priv = dev_get_priv(bus);
	int ret;

	ret = clk_get_by_index(bus, 0, &priv->clk);
	if (ret)
		return ret;

	ret = clk_enable(&priv->clk);
	if (ret && ret != -ENOSYS && ret != -ENOTSUPP)
		return ret;

	*rate = clk_get_rate(&priv->clk);
	if (!*rate)
		goto err_rate;

	debug("%s: get spi controller clk via device tree: %lu Hz\n",
	      __func__, *rate);

	return 0;

err_rate:
	clk_disable(&priv->clk);
	clk_free(&priv->clk);

	return -EINVAL;
#endif
}

static int dw_qspi_reset(struct udevice *bus)
{
	int ret;
	struct dw_qspi_priv *priv = dev_get_priv(bus);

	ret = reset_get_bulk(bus, &priv->resets);
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

static int dw_qspi_probe(struct udevice *bus)
{
	struct dw_qspi_platdata *plat = dev_get_platdata(bus);
	struct dw_qspi_priv *priv = dev_get_priv(bus);
	int ret;

	//close qspi0 XIP
	bst_close_xip(0, 0);
	//close qspi1 XIP
	bst_close_xip(1, 0);

	priv->regs = plat->regs;
	priv->freq = plat->frequency;

	ret = dw_qspi_get_clk(bus, &priv->bus_clk_rate);
	if (ret)
		return ret;

	ret = dw_qspi_reset(bus);
	if (ret)
		return ret;

	/* Currently only bits_per_word == 8 supported */
	priv->bits_per_word = 8;

	priv->tmode = 0; /* Tx & Rx */

	/* Basic HW init */
	qspi_hw_init(priv);

	return 0;
}

/* Return the max entries we can fill into tx fifo */
static inline u32 tx_max(struct dw_qspi_priv *priv)
{
	u32 tx_left, tx_room, rxtx_gap;

	tx_left = (priv->tx_end - priv->tx) / (priv->bits_per_word >> 3);
	tx_room = priv->fifo_len - dw_read(priv, DW_QSPI_TXFLR);

	/*
	 * Another concern is about the tx/rx mismatch, we
	 * thought about using (priv->fifo_len - rxflr - txflr) as
	 * one maximum value for tx, but it doesn't cover the
	 * data which is out of tx/rx fifo and inside the
	 * shift registers. So a control from sw point of
	 * view is taken.
	 */
	rxtx_gap = ((priv->rx_end - priv->rx) - (priv->tx_end - priv->tx)) /
		(priv->bits_per_word >> 3);

	return min3(tx_left, tx_room, (u32)(priv->fifo_len - rxtx_gap));
}

/* Return the max entries we should read out of rx fifo */
static inline u32 rx_max(struct dw_qspi_priv *priv)
{
	u32 rx_left = (priv->rx_end - priv->rx) / (priv->bits_per_word >> 3);

	return min_t(u32, rx_left, dw_read(priv, DW_QSPI_RXFLR));
}

static void dw_writer(struct dw_qspi_priv *priv)
{
	u32 max = tx_max(priv);
	u32 txw = 0;

	dw_write(priv, DW_QSPI_TXFLTR, (max - 1)<<16);
	while (max--) {
		while (!(dw_read(priv, DW_QSPI_SR) & SR_TF_NOT_FULL))
			;
		// Set the tx word if the transfer's original "tx"
		// is not null
		if (priv->tx_end - priv->len) {
			if (priv->bits_per_word == 8)
				txw = *(u8 *)(priv->tx);
			else if (priv->bits_per_word == 16)
				txw = *(u16 *)(priv->tx);
			else
				txw = *(u32 *)(priv->tx);
		}
		dw_write(priv, DW_QSPI_DR, txw);

		//debug("%s: tx=0x%02x\n", __func__, txw);
		priv->tx += priv->bits_per_word >> 3;
	}
	//printf(".\n\r");
}

static void dw_reader(struct dw_qspi_priv *priv)
{
	u32 max = rx_max(priv);
	u32 rxw;

	while (max--) {
		while (!(dw_read(priv, DW_QSPI_SR) & SR_RF_NOT_EMPT))
			;
		rxw = dw_read(priv, DW_QSPI_DR);
		//debug("%s: rx=0x%02x\n", __func__, rxw);

		/* Care about rx if the transfer's original "rx" is not null */
		if (priv->rx_end - priv->len)
			if (priv->bits_per_word == 8)
				*(u8 *)(priv->rx) = rxw;
			else if (priv->bits_per_word == 16)
				*(u8 *)(priv->rx) = rxw;
			else
				*(u32 *)(priv->rx) = rxw;

		priv->rx += priv->bits_per_word >> 3;
	}
}

static int poll_transfer(struct dw_qspi_priv *priv)
{
	do {
		WATCHDOG_RESET();
		dw_writer(priv);
		dw_reader(priv);
		WATCHDOG_RESET();
	} while (priv->rx_end > priv->rx);

	return 0;
}

/*
 * We define external_cs_manage function as 'weak' as some targets
 * (like MSCC Ocelot) don't control the external CS pin using a GPIO
 * controller. These SoCs use specific registers to control by
 * software the SPI pins (and especially the CS).
 */
__weak void external_cs_manage(struct udevice *dev, bool on)
{
#if defined(CONFIG_DM_GPIO)
	struct dw_qspi_priv *priv = dev_get_priv(dev->parent);

	if (!dm_gpio_is_valid(&priv->cs_gpio))
		return;

	dm_gpio_set_value(&priv->cs_gpio, on ? 1 : 0);
#endif
}

static int dw_qspi_xfer(struct udevice *dev, unsigned int bitlen,
		       const void *dout, void *din, unsigned long flags)
{
	struct udevice *bus = dev->parent;
	struct dw_qspi_priv *priv = dev_get_priv(bus);
	const u8 *tx = dout;
	u8 *rx = din;
	int ret = 0;
	u32 cr0 = 0;
	u32 val;
	u32 cs;
	u32 max = rx_max(priv);

	/* spi core configured to do 8 bit transfers */
	if (bitlen % 8) {
		debug("Non byte aligned SPI transfer.\n");
		return -1;
	}

	/* Start the transaction if necessary. */
	if (flags & SPI_XFER_BEGIN)
		external_cs_manage(dev, false);

	cr0 = (priv->bits_per_word - 1) | (priv->type << SPI_FRF_OFFSET) |
		(priv->mode << SPI_MODE_OFFSET) |
		(priv->tmode << SPI_TMOD_OFFSET);
//	cr0 = (32 - 1) | (0 << SPI_FRF_OFFSET) |
//		(0<< SPI_MODE_OFFSET) |
//		(1 << SPI_TMOD_OFFSET);

	if (rx && tx)
		priv->tmode = SPI_TMOD_EPROMREAD;
	else if (rx)
		priv->tmode = SPI_TMOD_RO;
	else
		/*
		 * In transmit only mode (SPI_TMOD_TO) input FIFO never gets
		 * any data which breaks our logic in poll_transfer() above.
		 */
		priv->tmode = SPI_TMOD_TR;

	cr0 &= ~SPI_TMOD_MASK;
	cr0 |= (priv->tmode << SPI_TMOD_OFFSET);

	priv->len = bitlen >> 3;
	debug("%s: rx=%p tx=%p len=%d [bytes]\n", __func__, rx, tx, priv->len);

//	if (tx)
		//mem_dump(tx,priv->len);

	priv->tx = (void *)tx;
	priv->tx_end = priv->tx + priv->len;
	priv->rx = rx;
	priv->rx_end = priv->rx + priv->len;

	/* Disable controller before writing control registers */
	spi_enable_chip(priv, 0);

	debug("%s: cr0=%08x\n", __func__, cr0);
	/* Reprogram cr0 only if changed */
	if (dw_read(priv, DW_QSPI_CTRL0) != cr0)
		dw_write(priv, DW_QSPI_CTRL0, cr0);


	/*
	 * Configure the desired SS (slave select 0...3) in the controller
	 * The DW SPI controller will activate and deactivate this CS
	 * automatically. So no cs_activate() etc is needed in this driver.
	 */
	cs = spi_chip_select(dev);
	//dw_write(priv, DW_QSPI_SER, 1 << cs);
	//dw_write(priv, DW_QSPI_TXFLTR,5 << 16);

	if (1) {
		dw_write(priv, DW_QSPI_CTRL1, 6);
		debug("%s: cr1=%08x\n", __func__, 6);
	} else{
		dw_write(priv, DW_QSPI_CTRL1, 0);
		debug("%s: cr1=%08x\n", __func__, 0);
	}
	//dw_write(priv, DW_QSPI_CTRL1, 6);
	/* Enable controller after writing control registers */
	spi_enable_chip(priv, 1);

	/* Start transfer in a polling loop */
	ret = poll_transfer(priv);

	/*
	 * Wait for current transmit operation to complete.
	 * Otherwise if some data still exists in Tx FIFO it can be
	 * silently flushed, i.e. dropped on disabling of the controller,
	 * which happens when writing 0 to DW_QSPI_SSIENR which happens
	 * in the beginning of new transfer.
	 */
	if (readl_poll_timeout(priv->regs + DW_QSPI_SR, val,
			       (val & SR_TF_EMPT) && !(val & SR_BUSY),
			       RX_TIMEOUT * 1000)) {
		ret = -ETIMEDOUT;
	}

	/* Stop the transaction if necessary */
	if (flags & SPI_XFER_END)
		external_cs_manage(dev, true);

	return ret;
}

static int dw_qspi_set_speed(struct udevice *bus, uint speed)
{
	struct dw_qspi_platdata *plat = bus->platdata;
	struct dw_qspi_priv *priv = dev_get_priv(bus);
	u16 clk_div;

	if (speed > plat->frequency)
		speed = plat->frequency;

	/* Disable controller before writing control registers */
	spi_enable_chip(priv, 0);

	/* clk_div doesn't support odd number */
	clk_div = priv->bus_clk_rate / speed;
	clk_div = (clk_div + 1) & 0xfffe;
	dw_write(priv, DW_QSPI_BAUDR, clk_div);

	/* Enable controller after writing control registers */
	spi_enable_chip(priv, 1);

	priv->freq = speed;
	debug("%s: regs=%p speed=%d clk_div=%d\n", __func__, priv->regs,
	      priv->freq, clk_div);

	return 0;
}

static int dw_qspi_set_mode(struct udevice *bus, uint mode)
{
	struct dw_qspi_priv *priv = dev_get_priv(bus);

	/*
	 * Can't set mode yet. Since this depends on if rx, tx, or
	 * rx & tx is requested. So we have to defer this to the
	 * real transfer function.
	 */
	priv->mode = mode;
	debug("%s: regs=%p, mode=%d\n", __func__, priv->regs, priv->mode);

	return 0;
}

static int dw_qspi_remove(struct udevice *bus)
{
	struct dw_qspi_priv *priv = dev_get_priv(bus);

	return reset_release_bulk(&priv->resets);
}

static const struct dm_spi_ops dw_qspi_ops = {
	.xfer		= dw_qspi_xfer,
	.set_speed	= dw_qspi_set_speed,
	.set_mode	= dw_qspi_set_mode,
	/*
	 * cs_info is not needed, since we require all chip selects to be
	 * in the device tree explicitly
	 */
};

static const struct udevice_id dw_qspi_ids[] = {
	{ .compatible = "snps,dw-ahb-ssi" },
	{ }
};

U_BOOT_DRIVER(dw_qspi) = {
	.name = "dw_qspi",
	.id = UCLASS_SPI,
	.of_match = dw_qspi_ids,
	.ops = &dw_qspi_ops,
	.ofdata_to_platdata = dw_qspi_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct dw_qspi_platdata),
	.priv_auto_alloc_size = sizeof(struct dw_qspi_priv),
	.probe = dw_qspi_probe,
	.remove = dw_qspi_remove,
};

