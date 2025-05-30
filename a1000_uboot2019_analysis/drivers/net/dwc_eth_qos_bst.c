// SPDX-License-Identifier: GPL-2.0
// * Copyright (c) 2016, NVIDIA CORPORATION.
// *
// * Portions based on U-Boot's rtl8169.c.

// * This driver supports the Synopsys Designware Ethernet QOS (Quality Of
// * Service) IP block. The IP supports multiple options for bus type, clocking/
// * reset structure, and feature list.
// *
// * The driver is written such that generic core logic is kept separate from
// * configuration-specific logic. Code that interacts with configuration-
// * specific resources is split out into separate functions to avoid polluting
// * common code. If/when this driver is enhanced to support multiple
// * configurations, the core code should be adapted to call all configuration-
// * specific functions through function pointers, with the definition of those
// * function pointers being supplied by struct udevice_id eqos_ids[]'s .data
// * field.
// *
// * The following configurations are currently supported:
// * tegra186:
// *    NVIDIA's Tegra186 chip. This configuration uses an AXI master/DMA bus,
// *    an AHB slave/register bus, contains the DMA, MTL, and MAC sub-blocks,
// *    and supports a single RGMII PHY. This configuration also has SW
// *    control over all clock and reset signals to the HW block.

//#define DEBUG

#include <common.h>
#include <clk.h>
#include <dm.h>
#include <errno.h>
#include <memalign.h>
#include <miiphy.h>
#include <net.h>
#include <netdev.h>
#include <phy.h>
#include <reset.h>
#include <wait_bit.h>
#include <asm/gpio.h>
#include <asm/io.h>

/* Core registers */

#define EQOS_MAC_REGS_BASE 0x000
struct eqos_mac_regs {
	u32 configuration;				/* 0x000 */
	u32 ex_configuration;			/* 0x004 */
	u32 filter;					/* 0x008 */
	u32 unused_004[(0x070 - 0x00c) / 4];	/* 0x00c */
	u32 q0_tx_flow_ctrl;			/* 0x070 */
	u32 unused_070[(0x090 - 0x074) / 4];	/* 0x074 */
	u32 rx_flow_ctrl;				/* 0x090 */
	u32 unused_094;				/* 0x094 */
	u32 txq_prty_map0;				/* 0x098 */
	u32 unused_09c;				/* 0x09c */
	u32 rxq_ctrl0;				/* 0x0a0 */
	u32 unused_0a4;				/* 0x0a4 */
	u32 rxq_ctrl2;				/* 0x0a8 */
	u32 unused_0ac[(0x0dc - 0x0ac) / 4];	/* 0x0ac */
	u32 us_tic_counter;			/* 0x0dc */
	u32 unused_0e0[(0x11c - 0x0e0) / 4];	/* 0x0e0 */
	u32 hw_feature0;				/* 0x11c */
	u32 hw_feature1;				/* 0x120 */
	u32 hw_feature2;				/* 0x124 */
	u32 unused_128[(0x200 - 0x128) / 4];	/* 0x128 */
	u32 mdio_address;				/* 0x200 */
	u32 mdio_data;				/* 0x204 */
	u32 unused_208[(0x300 - 0x208) / 4];	/* 0x208 */
	u32 address0_high;				/* 0x300 */
	u32 address0_low;				/* 0x304 */
};

#define EQOS_MAC_CONFIGURATION_GPSLCE			BIT(23)
#define EQOS_MAC_CONFIGURATION_CST			BIT(21)
#define EQOS_MAC_CONFIGURATION_ACS			BIT(20)
#define EQOS_MAC_CONFIGURATION_WD			BIT(19)
#define EQOS_MAC_CONFIGURATION_JD			BIT(17)
#define EQOS_MAC_CONFIGURATION_JE			BIT(16)
#define EQOS_MAC_CONFIGURATION_PS			BIT(15)
#define EQOS_MAC_CONFIGURATION_FES			BIT(14)
#define EQOS_MAC_CONFIGURATION_DM			BIT(13)
#define EQOS_MAC_CONFIGURATION_TE			BIT(1)
#define EQOS_MAC_CONFIGURATION_RE			BIT(0)

#define EQOS_MAC_PACKET_FILTER_RA			BIT(31)
#define EQOS_MAC_PACKET_FILTER_PROMODE			BIT(0)

#define EQOS_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT		16
#define EQOS_MAC_Q0_TX_FLOW_CTRL_PT_MASK		0xffff
#define EQOS_MAC_Q0_TX_FLOW_CTRL_TFE			BIT(1)

#define EQOS_MAC_RX_FLOW_CTRL_RFE			BIT(0)

#define EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_SHIFT		0
#define EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_MASK		0xff

#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_SHIFT			0
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_MASK			3
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_NOT_ENABLED		0
#define EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB		2

#define EQOS_MAC_RXQ_CTRL2_PSRQ0_SHIFT			0
#define EQOS_MAC_RXQ_CTRL2_PSRQ0_MASK			0xff

#define EQOS_MAC_HW_FEATURE1_TXFIFOSIZE_SHIFT		6
#define EQOS_MAC_HW_FEATURE1_TXFIFOSIZE_MASK		0x1f
#define EQOS_MAC_HW_FEATURE1_RXFIFOSIZE_SHIFT		0
#define EQOS_MAC_HW_FEATURE1_RXFIFOSIZE_MASK		0x1f

#define EQOS_MAC_MDIO_ADDRESS_PA_SHIFT			21
#define EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT			16
#define EQOS_MAC_MDIO_ADDRESS_CR_SHIFT			8
#define EQOS_MAC_MDIO_ADDRESS_CR_20_35			2
#define EQOS_MAC_MDIO_ADDRESS_CR_100_150		1
#define EQOS_MAC_MDIO_ADDRESS_CR_500_800		7
#define EQOS_MAC_MDIO_ADDRESS_SKAP			BIT(4)
#define EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT			2
#define EQOS_MAC_MDIO_ADDRESS_GOC_READ			3
#define EQOS_MAC_MDIO_ADDRESS_GOC_WRITE			1
#define EQOS_MAC_MDIO_ADDRESS_C45E			BIT(1)
#define EQOS_MAC_MDIO_ADDRESS_GB			BIT(0)

#define EQOS_MAC_MDIO_DATA_GD_MASK			0xffff

#define EQOS_MTL_REGS_BASE 0xd00
struct eqos_mtl_regs {
	u32 txq0_operation_mode;			/* 0xd00 */
	u32 unused_d04;				/* 0xd04 */
	u32 txq0_debug;				/* 0xd08 */
	u32 unused_d0c[(0xd18 - 0xd0c) / 4];	/* 0xd0c */
	u32 txq0_quantum_weight;			/* 0xd18 */
	u32 unused_d1c[(0xd30 - 0xd1c) / 4];	/* 0xd1c */
	u32 rxq0_operation_mode;			/* 0xd30 */
	u32 unused_d34;				/* 0xd34 */
	u32 rxq0_debug;				/* 0xd38 */
};

#define EQOS_MTL_TXQ0_OPERATION_MODE_TQS_SHIFT		16
#define EQOS_MTL_TXQ0_OPERATION_MODE_TQS_MASK		0x1ff
#define EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_SHIFT	2
#define EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_MASK		3
#define EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_ENABLED	2
#define EQOS_MTL_TXQ0_OPERATION_MODE_TSF		BIT(1)
#define EQOS_MTL_TXQ0_OPERATION_MODE_FTQ		BIT(0)

#define EQOS_MTL_TXQ0_DEBUG_TXQSTS			BIT(4)
#define EQOS_MTL_TXQ0_DEBUG_TRCSTS_SHIFT		1
#define EQOS_MTL_TXQ0_DEBUG_TRCSTS_MASK			3

#define EQOS_MTL_RXQ0_OPERATION_MODE_RQS_SHIFT		20
#define EQOS_MTL_RXQ0_OPERATION_MODE_RQS_MASK		0x3ff
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFD_SHIFT		14
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFD_MASK		0x3f
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFA_SHIFT		8
#define EQOS_MTL_RXQ0_OPERATION_MODE_RFA_MASK		0x3f
#define EQOS_MTL_RXQ0_OPERATION_MODE_EHFC		BIT(7)
#define EQOS_MTL_RXQ0_OPERATION_MODE_RSF		BIT(5)
#define EQOS_MTL_RXQ0_OPERATION_MODE_FEP		BIT(4)
#define EQOS_MTL_RXQ0_OPERATION_MODE_DISTCPEF	BIT(6)

#define EQOS_MTL_RXQ0_DEBUG_PRXQ_SHIFT			16
#define EQOS_MTL_RXQ0_DEBUG_PRXQ_MASK			0x7fff
#define EQOS_MTL_RXQ0_DEBUG_RXQSTS_SHIFT		4
#define EQOS_MTL_RXQ0_DEBUG_RXQSTS_MASK			3

#define EQOS_DMA_REGS_BASE 0x1000
struct eqos_dma_regs {
	u32 mode;					/* 0x1000 */
	u32 sysbus_mode;				/* 0x1004 */
	u32 unused_1008[(0x1100 - 0x1008) / 4];	/* 0x1008 */
	u32 ch0_control;				/* 0x1100 */
	u32 ch0_tx_control;			/* 0x1104 */
	u32 ch0_rx_control;			/* 0x1108 */
	u32 unused_110c;				/* 0x110c */
	u32 ch0_txdesc_list_haddress;		/* 0x1110 */
	u32 ch0_txdesc_list_address;		/* 0x1114 */
	u32 ch0_rxdesc_list_haddress;		/* 0x1118 */
	u32 ch0_rxdesc_list_address;		/* 0x111c */
	u32 ch0_txdesc_tail_pointer;		/* 0x1120 */
	u32 unused_1124;				/* 0x1124 */
	u32 ch0_rxdesc_tail_pointer;		/* 0x1128 */
	u32 ch0_txdesc_ring_length;		/* 0x112c */
	u32 ch0_rxdesc_ring_length;		/* 0x1130 */
};

#define EQOS_DMA_MODE_SWR				BIT(0)

#define EQOS_DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT		24
#define EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT		16
#define EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_MASK		0xf
#define EQOS_DMA_SYSBUS_MODE_EAME			BIT(11)
#define EQOS_DMA_SYSBUS_MODE_BLEN16			BIT(3)
#define EQOS_DMA_SYSBUS_MODE_BLEN8			BIT(2)
#define EQOS_DMA_SYSBUS_MODE_BLEN4			BIT(1)
#define EQOS_DMA_SYSBUS_MODE_BLEN4			BIT(1)
#define EQOS_DMA_SYSBUS_MODE_FB				BIT(0)

#define EQOS_DMA_CH0_CONTROL_PBLX8			BIT(16)

#define EQOS_DMA_CH0_TX_CONTROL_TXPBL_SHIFT		16
#define EQOS_DMA_CH0_TX_CONTROL_TXPBL_MASK		0x3f
#define EQOS_DMA_CH0_TX_CONTROL_OSP			BIT(4)
#define EQOS_DMA_CH0_TX_CONTROL_ST			BIT(0)

#define EQOS_DMA_CH0_RX_CONTROL_RXPBL_SHIFT		16
#define EQOS_DMA_CH0_RX_CONTROL_RXPBL_MASK		0x3f
#define EQOS_DMA_CH0_RX_CONTROL_RBSZ_SHIFT		1
#define EQOS_DMA_CH0_RX_CONTROL_RBSZ_MASK		0x3fff
#define EQOS_DMA_CH0_RX_CONTROL_SR			BIT(0)

/* These registers are Tegra186-specific */
#define EQOS_TEGRA186_REGS_BASE 0x8800
struct eqos_regs {
	u32 sdmemcomppadctrl;			/* 0x8800 */
	u32 auto_cal_config;			/* 0x8804 */
	u32 unused_8808;				/* 0x8808 */
	u32 auto_cal_status;			/* 0x880c */
};

#define EQOS_SDMEMCOMPPADCTRL_PAD_E_INPUT_OR_E_PWRD	BIT(31)

#define EQOS_AUTO_CAL_CONFIG_START			BIT(31)
#define EQOS_AUTO_CAL_CONFIG_ENABLE			BIT(29)

#define EQOS_AUTO_CAL_STATUS_ACTIVE			BIT(31)

/* Descriptors */

#define EQOS_DESCRIPTOR_WORDS	4
#define EQOS_DESCRIPTOR_SIZE	(EQOS_DESCRIPTOR_WORDS * 4)
/* We assume ARCH_DMA_MINALIGN >= 16; 16 is the EQOS HW minimum */
#define EQOS_DESCRIPTOR_ALIGN	ARCH_DMA_MINALIGN
#define EQOS_DESCRIPTORS_TX	4
#define EQOS_DESCRIPTORS_RX	4
#define EQOS_DESCRIPTORS_NUM	(EQOS_DESCRIPTORS_TX + EQOS_DESCRIPTORS_RX)
#define EQOS_DESCRIPTORS_SIZE	ALIGN(EQOS_DESCRIPTORS_NUM * \
				      EQOS_DESCRIPTOR_SIZE, ARCH_DMA_MINALIGN)
#define EQOS_BUFFER_ALIGN	ARCH_DMA_MINALIGN
#define EQOS_MAX_PACKET_SIZE	ALIGN(1568, ARCH_DMA_MINALIGN)
#define EQOS_RX_BUFFER_SIZE	(EQOS_DESCRIPTORS_RX * EQOS_MAX_PACKET_SIZE)

// * Warn if the cache-line size is larger than the descriptor size. In such
// * cases the driver will likely fail because the CPU needs to flush the cache
// * when requeuing RX buffers, therefore descriptors written by the hardware
// * may be discarded. Architectures with full IO coherence, such as x86, do not
// * experience this issue, and hence are excluded from this condition.
// *
// * This can be fixed by defining CONFIG_SYS_NONCACHED_MEMORY which will cause
// * the driver to allocate descriptors from a pool of non-cached memory.
#if EQOS_DESCRIPTOR_SIZE < ARCH_DMA_MINALIGN
#if !defined(CONFIG_SYS_NONCACHED_MEMORY) && \
	!defined(CONFIG_SYS_DCACHE_OFF) && !defined(CONFIG_X86)
#warning Cache line size is larger than descriptor size
#endif
#endif

struct eqos_desc {
	u32 des0;
	u32 des1;
	u32 des2;
	u32 des3;
};

#define EQOS_DESC3_OWN		BIT(31)
#define EQOS_DESC3_FD		BIT(29)
#define EQOS_DESC3_LD		BIT(28)
#define EQOS_DESC3_BUF1V	BIT(24)

enum board_type {
	BSTA1000B_EVB = 0,
	BSTA1000B_FADA,
	BSTA1000B_FADB,
};

#define UBOOT_CLK_SET	0
#define TXRX_FLOW_CTRL	0

struct eqos_config {
	bool reg_access_always_ok;
};

struct eqos_priv {
	struct udevice *dev;
	const struct eqos_config *config;
	fdt_addr_t regs;
	struct eqos_mac_regs *mac_regs;
	struct eqos_mtl_regs *mtl_regs;
	struct eqos_dma_regs *dma_regs;
	struct reset_ctl_bulk reset_ctl;
	struct gpio_desc phy_reset_gpio;
	struct clk clk_master_bus;
	struct clk clk_rx;
	struct clk clk_ptp_ref;
	struct clk clk_tx;
	struct clk clk_slave_bus;
	struct mii_dev *mii;
	struct phy_device *phy;
	void *descs;
	struct eqos_desc *tx_descs;
	struct eqos_desc *rx_descs;
	int tx_desc_idx, rx_desc_idx;
	void *tx_dma_buf;
	void *rx_dma_buf;
	void *rx_pkt;
	bool started;
	bool reg_access_ok;
	int phy_interface;
	int phy_addr;
	int board;
};

// * TX and RX descriptors are 16 bytes. This causes problems with the cache
// * maintenance on CPUs where the cache-line size exceeds the size of these
// * descriptors. What will happen is that when the driver receives a packet
// * it will be immediately requeued for the hardware to reuse. The CPU will
// * therefore need to flush the cache-line containing the descriptor, which
// * will cause all other descriptors in the same cache-line to be flushed
// * along with it. If one of those descriptors had been written to by the
// * device those changes (and the associated packet) will be lost.
// *
// * To work around this, we make use of non-cached memory if available. If
// * descriptors are mapped uncached there's no need to manually flush them
// * or invalidate them.
// *
// * Note that this only applies to descriptors. The packet data buffers do
// * not have the same constraints since they are 1536 bytes large, so they
// * are unlikely to share cache-lines.
static void *eqos_alloc_descs(unsigned int num)
{
#ifdef CONFIG_SYS_NONCACHED_MEMORY
	return (void *)noncached_alloc(EQOS_DESCRIPTORS_SIZE,
				       EQOS_DESCRIPTOR_ALIGN);
#else
	return memalign(EQOS_DESCRIPTOR_ALIGN, EQOS_DESCRIPTORS_SIZE);
#endif
}

static void eqos_free_descs(void *descs)
{
#ifdef CONFIG_SYS_NONCACHED_MEMORY
	/* FIXME: noncached_alloc() has no opposite */
#else
	free(descs);
#endif
}

static void eqos_inval_desc(void *desc)
{
#ifndef CONFIG_SYS_NONCACHED_MEMORY
	unsigned long start = (unsigned long)desc & ~(ARCH_DMA_MINALIGN - 1);
	unsigned long end = ALIGN(start + EQOS_DESCRIPTOR_SIZE,
				  ARCH_DMA_MINALIGN);

	invalidate_dcache_range(start, end);
#endif
}

static void eqos_flush_desc(void *desc)
{
#ifndef CONFIG_SYS_NONCACHED_MEMORY
	flush_cache((unsigned long)desc, EQOS_DESCRIPTOR_SIZE);
#endif
}

static void eqos_inval_buffer(void *buf, size_t size)
{
	unsigned long start = (unsigned long)buf & ~(ARCH_DMA_MINALIGN - 1);
	unsigned long end = ALIGN(start + size, ARCH_DMA_MINALIGN);
#ifndef CONFIG_SYS_NONCACHED_MEMORY
	invalidate_dcache_range(start, end);
#endif
}

static void eqos_flush_buffer(void *buf, size_t size)
{
#ifndef CONFIG_SYS_NONCACHED_MEMORY
	flush_cache((unsigned long)buf, size);
#endif
}

static int eqos_mdio_wait_idle(struct eqos_priv *eqos)
{
	return wait_for_bit_le32(&eqos->mac_regs->mdio_address,
				 EQOS_MAC_MDIO_ADDRESS_GB, false,
				 1000000, true);
}

static int eqos_mdio_read(struct mii_dev *bus, int mdio_addr, int mdio_devad,
			  int mdio_reg)
{
	struct eqos_priv *eqos = bus->priv;
	u32 val;
	int ret;

	//debug("%s(dev=%p, addr=%x, reg=%d):\n",
	//__func__, eqos->dev, mdio_addr,
	 //     mdio_reg);

	ret = eqos_mdio_wait_idle(eqos);
	if (ret) {
		pr_err("MDIO not idle at entry");
		return ret;
	}

	if (mdio_devad == MDIO_DEVAD_NONE) {
		val = readl(&eqos->mac_regs->mdio_address);
		val &= EQOS_MAC_MDIO_ADDRESS_SKAP;
		val &= ~EQOS_MAC_MDIO_ADDRESS_C45E;
		val |= (mdio_addr << EQOS_MAC_MDIO_ADDRESS_PA_SHIFT) |
			(mdio_reg << EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT) |
			(EQOS_MAC_MDIO_ADDRESS_CR_100_150 <<
			 EQOS_MAC_MDIO_ADDRESS_CR_SHIFT) |
			(EQOS_MAC_MDIO_ADDRESS_GOC_READ <<
			 EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT) |
			EQOS_MAC_MDIO_ADDRESS_GB;
		writel(val, &eqos->mac_regs->mdio_address);
	} else {
		writel((mdio_reg & 0xffff) << 16, &eqos->mac_regs->mdio_data);
		val = readl(&eqos->mac_regs->mdio_address);
		val &= EQOS_MAC_MDIO_ADDRESS_SKAP;
		val |= EQOS_MAC_MDIO_ADDRESS_C45E;
		val |= (mdio_addr << EQOS_MAC_MDIO_ADDRESS_PA_SHIFT) |
			(mdio_devad << EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT) |
			(EQOS_MAC_MDIO_ADDRESS_CR_100_150 <<
			 EQOS_MAC_MDIO_ADDRESS_CR_SHIFT) |
			(EQOS_MAC_MDIO_ADDRESS_GOC_READ <<
			 EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT) |
			EQOS_MAC_MDIO_ADDRESS_GB;
		writel(val, &eqos->mac_regs->mdio_address);
	}
	mdelay(1);

	ret = eqos_mdio_wait_idle(eqos);
	if (ret) {
		pr_err("MDIO read didn't complete");
		return ret;
	}

	val = readl(&eqos->mac_regs->mdio_data);
	val &= EQOS_MAC_MDIO_DATA_GD_MASK;

//	debug("%s: val=%x\n", __func__, val);

	return val;
}

static int eqos_mdio_write(struct mii_dev *bus, int mdio_addr, int mdio_devad,
			   int mdio_reg, u16 mdio_val)
{
	struct eqos_priv *eqos = bus->priv;
	u32 val;
	int ret;

//	debug("%s(dev=%p, addr=%x, reg=%d, val=%x):\n", __func__, eqos->dev,
//	      mdio_addr, mdio_reg, mdio_val);

	ret = eqos_mdio_wait_idle(eqos);
	if (ret) {
		pr_err("MDIO not idle at entry");
		return ret;
	}
	if (mdio_devad == MDIO_DEVAD_NONE) {
		writel(mdio_val, &eqos->mac_regs->mdio_data);

		val = readl(&eqos->mac_regs->mdio_address);
		val &= EQOS_MAC_MDIO_ADDRESS_SKAP;
		val &= ~EQOS_MAC_MDIO_ADDRESS_C45E;
		val |= (mdio_addr << EQOS_MAC_MDIO_ADDRESS_PA_SHIFT) |
			(mdio_reg << EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT) |
			(EQOS_MAC_MDIO_ADDRESS_CR_100_150 <<
			 EQOS_MAC_MDIO_ADDRESS_CR_SHIFT) |
			(EQOS_MAC_MDIO_ADDRESS_GOC_WRITE <<
			 EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT) |
			EQOS_MAC_MDIO_ADDRESS_GB;
		writel(val, &eqos->mac_regs->mdio_address);
	} else {
		writel((mdio_val & 0xffff) | ((mdio_reg & 0xffff) << 16),
		       &eqos->mac_regs->mdio_data);
		val = readl(&eqos->mac_regs->mdio_address);
		val &= EQOS_MAC_MDIO_ADDRESS_SKAP;
		val |= EQOS_MAC_MDIO_ADDRESS_C45E;
		val |= (mdio_addr << EQOS_MAC_MDIO_ADDRESS_PA_SHIFT) |
			(mdio_devad << EQOS_MAC_MDIO_ADDRESS_RDA_SHIFT) |
			(EQOS_MAC_MDIO_ADDRESS_CR_100_150 <<
			 EQOS_MAC_MDIO_ADDRESS_CR_SHIFT) |
			(EQOS_MAC_MDIO_ADDRESS_GOC_WRITE <<
			 EQOS_MAC_MDIO_ADDRESS_GOC_SHIFT) |
			EQOS_MAC_MDIO_ADDRESS_GB;
		writel(val, &eqos->mac_regs->mdio_address);
	}
	mdelay(1);

	ret = eqos_mdio_wait_idle(eqos);
	if (ret) {
		pr_err("MDIO read didn't complete");
		return ret;
	}

	return 0;
}

static int eqos_start_clks(struct udevice *dev)
{
#if UBOOT_CLK_SET
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = clk_enable(&eqos->clk_slave_bus);
	if (ret < 0) {
		pr_err("clk_enable(clk_slave_bus) failed: %d", ret);
		goto err;
	}

	ret = clk_enable(&eqos->clk_master_bus);
	if (ret < 0) {
		pr_err("clk_enable(clk_master_bus) failed: %d", ret);
		goto err_disable_clk_slave_bus;
	}

	ret = clk_enable(&eqos->clk_rx);
	if (ret < 0) {
		pr_err("clk_enable(clk_rx) failed: %d", ret);
		goto err_disable_clk_master_bus;
	}

	ret = clk_enable(&eqos->clk_ptp_ref);
	if (ret < 0) {
		pr_err("clk_enable(clk_ptp_ref) failed: %d", ret);
		goto err_disable_clk_rx;
	}

	ret = clk_set_rate(&eqos->clk_ptp_ref, 125 * 1000 * 1000);
	if (ret < 0) {
		pr_err("clk_set_rate(clk_ptp_ref) failed: %d", ret);
		goto err_disable_clk_ptp_ref;
	}

	ret = clk_enable(&eqos->clk_tx);
	if (ret < 0) {
		pr_err("clk_enable(clk_tx) failed: %d", ret);
		goto err_disable_clk_ptp_ref;
	}

	debug("%s: OK\n", __func__);
	return 0;

err_disable_clk_ptp_ref:
	clk_disable(&eqos->clk_ptp_ref);
err_disable_clk_rx:
	clk_disable(&eqos->clk_rx);
err_disable_clk_master_bus:
	clk_disable(&eqos->clk_master_bus);
err_disable_clk_slave_bus:
	clk_disable(&eqos->clk_slave_bus);
err:
	debug("%s: FAILED: %d\n", __func__, ret);

	return ret;
#else
	return 0;
#endif
}

void eqos_stop_clks(struct udevice *dev)
{
#if UBOOT_CLK_SET
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	clk_disable(&eqos->clk_tx);
	clk_disable(&eqos->clk_ptp_ref);
	clk_disable(&eqos->clk_rx);
	clk_disable(&eqos->clk_master_bus);
	clk_disable(&eqos->clk_slave_bus);

	debug("%s: OK\n", __func__);
#endif
}

static int eqos_start_resets(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	ret = dm_gpio_set_value(&eqos->phy_reset_gpio, 0);
	if (ret < 0) {
		pr_err("dm_gpio_set_value(phy_reset, assert) failed: %d", ret);
		return ret;
	}

	mdelay(2);

	ret = dm_gpio_set_value(&eqos->phy_reset_gpio, 1);
	if (ret < 0) {
		pr_err("dm_gpio_set_value(phy_reset, deassert) failed: %d",
		       ret);
		return ret;
	}

	ret = reset_get_bulk(dev, &eqos->reset_ctl);
	if (ret) {
		/* Return 0 if error due to !CONFIG_DM_RESET and reset
		 * DT property is not present.
		 */
		if (ret == -ENOENT || ret == -ENOTSUPP)
			return 0;

		pr_err("Can't get reset: %d\n", ret);
		return ret;
	}

	ret = reset_deassert_bulk(&eqos->reset_ctl);
	if (ret) {
		reset_release_bulk(&eqos->reset_ctl);
		pr_err("Failed to reset: %d\n", ret);
		return ret;
	}

	debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_stop_resets(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	ret = reset_get_bulk(dev, &eqos->reset_ctl);
	if (ret) {
		/* Return 0 if error due to !CONFIG_DM_RESET and reset
		 * DT property is not present.
		 */
		if (ret == -ENOENT || ret == -ENOTSUPP)
			return 0;

		pr_err("Can't get reset: %d\n", ret);
		return ret;
	}
	dm_gpio_set_value(&eqos->phy_reset_gpio, 0);

	return 0;
}

static ulong eqos_get_tick_clk_rate(struct udevice *dev)
{
	//struct eqos_priv *eqos = dev_get_priv(dev);

	return (125 * 1000 * 1000);
	//return clk_get_rate(&eqos->clk_slave_bus);
}

static int eqos_set_full_duplex(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	setbits_le32(&eqos->mac_regs->configuration, EQOS_MAC_CONFIGURATION_DM);

	return 0;
}

static int eqos_set_half_duplex(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	clrbits_le32(&eqos->mac_regs->configuration, EQOS_MAC_CONFIGURATION_DM);

	/* WAR: Flush TX queue when switching to half-duplex */
	setbits_le32(&eqos->mtl_regs->txq0_operation_mode,
		     EQOS_MTL_TXQ0_OPERATION_MODE_FTQ);

	return 0;
}

static int eqos_set_gmii_speed(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	clrbits_le32(&eqos->mac_regs->configuration,
		     EQOS_MAC_CONFIGURATION_PS | EQOS_MAC_CONFIGURATION_FES);

	return 0;
}

static int eqos_set_mii_speed_100(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	setbits_le32(&eqos->mac_regs->configuration,
		     EQOS_MAC_CONFIGURATION_PS | EQOS_MAC_CONFIGURATION_FES);

	return 0;
}

static int eqos_set_mii_speed_10(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	clrsetbits_le32(&eqos->mac_regs->configuration,
			EQOS_MAC_CONFIGURATION_FES, EQOS_MAC_CONFIGURATION_PS);

	return 0;
}

static int eqos_set_tx_clk_speed(struct udevice *dev)
{
#if UBOOT_CLK_SET
	struct eqos_priv *eqos = dev_get_priv(dev);
	ulong rate;
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	switch (eqos->phy->speed) {
	case SPEED_1000:
		rate = 125 * 1000 * 1000;
		break;
	case SPEED_100:
		rate = 25 * 1000 * 1000;
		break;
	case SPEED_10:
		rate = 2.5 * 1000 * 1000;
		break;
	default:
		pr_err("invalid speed %d", eqos->phy->speed);
		return -EINVAL;
	}

	ret = clk_set_rate(&eqos->clk_tx, rate);
	if (ret < 0) {
		pr_err("clk_set_rate(tx_clk, %lu) failed: %d", rate, ret);
		return ret;
	}
#endif
	return 0;
}

static int eqos_adjust_link(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	if (eqos->phy->duplex)
		ret = eqos_set_full_duplex(dev);
	else
		ret = eqos_set_half_duplex(dev);
	if (ret < 0) {
		pr_err("eqos_set_*_duplex() failed: %d", ret);
		return ret;
	}

	switch (eqos->phy->speed) {
	case SPEED_1000:
		ret = eqos_set_gmii_speed(dev);
		break;
	case SPEED_100:
		ret = eqos_set_mii_speed_100(dev);
		break;
	case SPEED_10:
		ret = eqos_set_mii_speed_10(dev);
		break;
	default:
		pr_err("invalid speed %d", eqos->phy->speed);
		return -EINVAL;
	}
	if (ret < 0) {
		pr_err("eqos_set_*mii_speed*() failed: %d", ret);
		return ret;
	}

	ret = eqos_set_tx_clk_speed(dev);
	if (ret < 0) {
		pr_err("eqos_set_tx_clk_speed() failed: %d", ret);
		return ret;
	}

	return 0;
}

static int eqos_write_hwaddr(struct udevice *dev)
{
	struct eth_pdata *plat = dev_get_platdata(dev);
	struct eqos_priv *eqos = dev_get_priv(dev);
	u32 val;

//	 * This function may be called before start() or after stop(). At that
//	 * time, on at least some configurations of the EQoS HW, all clocks to
//	 * the EQoS HW block will be stopped, and a reset signal applied. If
//	 * any register access is attempted in this state, bus timeouts or CPU
//	 * hangs may occur. This check prevents that.
//	 *
//	 * A simple solution to this problem would be to not implement
//	 * write_hwaddr(), since start() always writes the MAC address into HW
//	 * anyway. However, it is desirable to implement write_hwaddr() to
//	 * support the case of SW that runs subsequent to U-Boot which expects
//	 * the MAC address to already be programmed into the EQoS registers,
//	 * which must happen irrespective of whether the U-Boot user (or
//	 * scripts) actually made use of the EQoS device, and hence
//	 * irrespective of whether start() was ever called.
//	 *
//	 * Note that this requirement by subsequent SW is not valid for
//	 * Tegra186, and is likely not valid for any non-PCI instantiation of
//	 * the EQoS HW block. This function is implemented solely as
//	 * future-proofing with the expectation the driver will eventually be
//	 * ported to some system where the expectation above is true.
	if (!eqos->config->reg_access_always_ok && !eqos->reg_access_ok)
		return 0;

	/* Update the MAC address */
	val = (plat->enetaddr[5] << 8) |
		(plat->enetaddr[4]);
	writel(val, &eqos->mac_regs->address0_high);
	val = (plat->enetaddr[3] << 24) |
		(plat->enetaddr[2] << 16) |
		(plat->enetaddr[1] << 8) |
		(plat->enetaddr[0]);
	writel(val, &eqos->mac_regs->address0_low);

	return 0;
}

static int get_board_type(void)
{
	const char *ch;

	ch = env_get("board_name");
	if (!ch) {
		pr_err("no board_name in env\n");
		return -1;
	}

	if (!strcmp(ch, "bsta1000b-fada"))
		return BSTA1000B_FADA;
	else if (!strcmp(ch, "bsta1000b-fadb"))
		return BSTA1000B_FADB;

	return BSTA1000B_EVB;
}

static int eqos_start(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret, i;
	ulong rate;
	u32 val, tx_fifo_sz, rx_fifo_sz, tqs, rqs, pbl;
	ulong last_rx_desc;

	debug("%s(dev=%p):\n", __func__, dev);

	//dcache_disable();

	eqos->tx_desc_idx = 0;
	eqos->rx_desc_idx = 0;

	ret = eqos_start_clks(dev);
	if (ret < 0) {
		pr_err("eqos_start_clks() failed: %d", ret);
		goto err;
	}

	ret = eqos_start_resets(dev);
	if (ret < 0) {
		pr_err("eqos_start_resets() failed: %d", ret);
		goto err_stop_clks;
	}

	mdelay(1);

	eqos->reg_access_ok = true;

	rate = eqos_get_tick_clk_rate(dev);
	val = (rate / 1000000) - 1;
	writel(val, &eqos->mac_regs->us_tic_counter);

	eqos->phy = phy_connect(eqos->mii, eqos->phy_addr,
				dev, eqos->phy_interface);
	if (!eqos->phy) {
		pr_err("phy_connect() failed");
		goto err_stop_resets;
	}

	if (eqos->board == BSTA1000B_FADA || eqos->board == BSTA1000B_FADB)
		eqos->phy->flags |= PHY_FIBER_MODE;

	ret = phy_config(eqos->phy);
	if (ret < 0) {
		pr_err("phy_config() failed: %d", ret);
		goto err_shutdown_phy;
	}
	ret = phy_startup(eqos->phy);
	if (ret < 0) {
		pr_err("phy_startup() failed: %d", ret);
		goto err_shutdown_phy;
	}

	if (!eqos->phy->link) {
		pr_err("No link");
		goto err_shutdown_phy;
	}

	ret = eqos_adjust_link(dev);
	if (ret < 0) {
		pr_err("eqos_adjust_link() failed: %d", ret);
		goto err_shutdown_phy;
	}

	/* bcm89881 phy needs modify rx delay*/
	//if (eqos->phy_addr == 0x2)
	//	phy_extend_op(eqos->phy);

	setbits_le32(&eqos->dma_regs->mode, EQOS_DMA_MODE_SWR);

	ret = wait_for_bit_le32(&eqos->dma_regs->mode,
				EQOS_DMA_MODE_SWR, false, 5000, false);
	if (ret) {
		pr_err("EQOS_DMA_MODE_SWR stuck\n");
		goto err_shutdown_phy;
	}
	//open parity
	val = readl(0x33000080);
	if (val & (1 << 7)) {
		setbits_le32((char *)(eqos->mac_regs) + 0xce0, 0x5);
		mdelay(1);
		setbits_le32((char *)(eqos->mac_regs) + 0xcc0, 0x11f);
		mdelay(1);
		setbits_le32((char *)(eqos->mac_regs) + 0xcc8, 0x1111);
		mdelay(1);
	}
	/* Configure MTL */

	/* Enable Store and Forward mode for TX */
	/* Program Tx operating mode */
	setbits_le32(&eqos->mtl_regs->txq0_operation_mode,
		     EQOS_MTL_TXQ0_OPERATION_MODE_TSF |
		     (EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_ENABLED <<
		      EQOS_MTL_TXQ0_OPERATION_MODE_TXQEN_SHIFT));

	/* Transmit Queue weight */
	writel(0x10, &eqos->mtl_regs->txq0_quantum_weight);

	/* Enable Store and Forward mode for RX, since no jumbo frame */
	setbits_le32(&eqos->mtl_regs->rxq0_operation_mode,
		     EQOS_MTL_RXQ0_OPERATION_MODE_RSF);

	/* Enable Forward Error packet. */
	/* (when use zebu, no TCP packet to test.) */
	setbits_le32(&eqos->mtl_regs->rxq0_operation_mode,
		     EQOS_MTL_RXQ0_OPERATION_MODE_FEP |
			 EQOS_MTL_RXQ0_OPERATION_MODE_DISTCPEF);

	/* Transmit/Receive queue fifo size; use all RAM for 1 queue */
	val = readl(&eqos->mac_regs->hw_feature1);
	tx_fifo_sz = (val >> EQOS_MAC_HW_FEATURE1_TXFIFOSIZE_SHIFT) &
		EQOS_MAC_HW_FEATURE1_TXFIFOSIZE_MASK;
	rx_fifo_sz = (val >> EQOS_MAC_HW_FEATURE1_RXFIFOSIZE_SHIFT) &
		EQOS_MAC_HW_FEATURE1_RXFIFOSIZE_MASK;

	/* r/tx_fifo_sz is encoded as log2(n / 128). Undo that by shifting. */
	/* r/tqs is encoded as (n / 256) - 1. */
	tqs = (128 << tx_fifo_sz) / 256 - 1;
	rqs = (128 << rx_fifo_sz) / 256 - 1;

	clrsetbits_le32(&eqos->mtl_regs->txq0_operation_mode,
			EQOS_MTL_TXQ0_OPERATION_MODE_TQS_MASK <<
			EQOS_MTL_TXQ0_OPERATION_MODE_TQS_SHIFT,
			tqs << EQOS_MTL_TXQ0_OPERATION_MODE_TQS_SHIFT);
	clrsetbits_le32(&eqos->mtl_regs->rxq0_operation_mode,
			EQOS_MTL_RXQ0_OPERATION_MODE_RQS_MASK <<
			EQOS_MTL_RXQ0_OPERATION_MODE_RQS_SHIFT,
			rqs << EQOS_MTL_RXQ0_OPERATION_MODE_RQS_SHIFT);

	/* Flow control used only if each channel gets 4KB or more FIFO */
	if (rqs >= ((4096 / 256) - 1)) {
		u32 rfd, rfa;

		setbits_le32(&eqos->mtl_regs->rxq0_operation_mode,
			     EQOS_MTL_RXQ0_OPERATION_MODE_EHFC);

		/* Set Threshold for Activating Flow Control space for min 2 */
		/* frames ie, (1500 * 1) = 1500 bytes. */
		/* Set Threshold for Deactivating Flow Control for space of */
		/* min 1 frame (frame size 1500bytes) in receive fifo */
		if (rqs == ((4096 / 256) - 1)) {
			/* This violates the above formula */
			/* because of FIFO size */
			/* limit therefore overflow */
			/* may occur inspite of this. */
			rfd = 0x3;	/* Full-3K */
			rfa = 0x1;	/* Full-1.5K */
		} else if (rqs == ((8192 / 256) - 1)) {
			rfd = 0x6;	/* Full-4K */
			rfa = 0xa;	/* Full-6K */
		} else if (rqs == ((16384 / 256) - 1)) {
			rfd = 0x6;	/* Full-4K */
			rfa = 0x12;	/* Full-10K */
		} else {
			rfd = 0x6;	/* Full-4K */
			rfa = 0x1E;	/* Full-16K */
		}

		clrsetbits_le32(&eqos->mtl_regs->rxq0_operation_mode,
				(EQOS_MTL_RXQ0_OPERATION_MODE_RFD_MASK <<
				 EQOS_MTL_RXQ0_OPERATION_MODE_RFD_SHIFT) |
				(EQOS_MTL_RXQ0_OPERATION_MODE_RFA_MASK <<
				 EQOS_MTL_RXQ0_OPERATION_MODE_RFA_SHIFT),
				(rfd <<
				 EQOS_MTL_RXQ0_OPERATION_MODE_RFD_SHIFT) |
				(rfa <<
				 EQOS_MTL_RXQ0_OPERATION_MODE_RFA_SHIFT));
	}

	/* Configure MAC */

	clrsetbits_le32(&eqos->mac_regs->rxq_ctrl0,
			EQOS_MAC_RXQ_CTRL0_RXQ0EN_MASK <<
			EQOS_MAC_RXQ_CTRL0_RXQ0EN_SHIFT,
			EQOS_MAC_RXQ_CTRL0_RXQ0EN_ENABLED_DCB <<
			EQOS_MAC_RXQ_CTRL0_RXQ0EN_SHIFT);

	/* Set TX flow control parameters */
	/* Set Pause Time */
	/* Assign priority for TX flow control */
	clrbits_le32(&eqos->mac_regs->txq_prty_map0,
		     EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_MASK <<
		     EQOS_MAC_TXQ_PRTY_MAP0_PSTQ0_SHIFT);
	/* Assign priority for RX flow control */
	clrbits_le32(&eqos->mac_regs->rxq_ctrl2,
		     EQOS_MAC_RXQ_CTRL2_PSRQ0_MASK <<
	     EQOS_MAC_RXQ_CTRL2_PSRQ0_SHIFT);
#if TXRX_FLOW_CTRL
	setbits_le32(&eqos->mac_regs->q0_tx_flow_ctrl,
		     0xffff << EQOS_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT);
	/* Enable flow control */
	setbits_le32(&eqos->mac_regs->q0_tx_flow_ctrl,
		     EQOS_MAC_Q0_TX_FLOW_CTRL_TFE);
	setbits_le32(&eqos->mac_regs->rx_flow_ctrl,
		     EQOS_MAC_RX_FLOW_CTRL_RFE);
#else
	clrbits_le32(&eqos->mac_regs->q0_tx_flow_ctrl,
		     0xffff << EQOS_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT);
	/* Disable flow control */
	clrbits_le32(&eqos->mac_regs->q0_tx_flow_ctrl,
		     EQOS_MAC_Q0_TX_FLOW_CTRL_TFE);
	clrbits_le32(&eqos->mac_regs->rx_flow_ctrl,
		     EQOS_MAC_RX_FLOW_CTRL_RFE);
#endif
	clrsetbits_le32(&eqos->mac_regs->configuration,
			0xffffffff, EQOS_MAC_CONFIGURATION_DM);

//	clrsetbits_le32(&eqos->mac_regs->configuration,
//			EQOS_MAC_CONFIGURATION_GPSLCE |
//			EQOS_MAC_CONFIGURATION_WD |
//			EQOS_MAC_CONFIGURATION_JD |
//			EQOS_MAC_CONFIGURATION_JE,
//			EQOS_MAC_CONFIGURATION_CST |
//			EQOS_MAC_CONFIGURATION_ACS);
	eqos_write_hwaddr(dev);

	/* Configure DMA */

	/* Enable OSP mode */
	setbits_le32(&eqos->dma_regs->ch0_tx_control,
		     EQOS_DMA_CH0_TX_CONTROL_OSP);

	/* RX buffer size. Must be a multiple of bus width */
	clrsetbits_le32(&eqos->dma_regs->ch0_rx_control,
			EQOS_DMA_CH0_RX_CONTROL_RBSZ_MASK <<
			EQOS_DMA_CH0_RX_CONTROL_RBSZ_SHIFT,
			EQOS_MAX_PACKET_SIZE <<
			EQOS_DMA_CH0_RX_CONTROL_RBSZ_SHIFT);

	setbits_le32(&eqos->dma_regs->ch0_control,
		     EQOS_DMA_CH0_CONTROL_PBLX8);

	/* Burst length must be < 1/2 FIFO size. */
	/* FIFO size in tqs is encoded as (n / 256) - 1. */
	/* Each burst is n * 8 (PBLX8) * 16 (AXI width) == 128 bytes. */
	/* Half of n * 256 is n * 128, so pbl == tqs, modulo the -1. */
	pbl = tqs + 1;
	if (pbl > 32)
		pbl = 32;
	clrsetbits_le32(&eqos->dma_regs->ch0_tx_control,
			EQOS_DMA_CH0_TX_CONTROL_TXPBL_MASK <<
			EQOS_DMA_CH0_TX_CONTROL_TXPBL_SHIFT,
			pbl << EQOS_DMA_CH0_TX_CONTROL_TXPBL_SHIFT);

	clrsetbits_le32(&eqos->dma_regs->ch0_rx_control,
			EQOS_DMA_CH0_RX_CONTROL_RXPBL_MASK <<
			EQOS_DMA_CH0_RX_CONTROL_RXPBL_SHIFT,
			0x10 << EQOS_DMA_CH0_RX_CONTROL_RXPBL_SHIFT);

	/* DMA performance configuration */
	val = (0xf << EQOS_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT) |
		 (0xf << EQOS_DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT) |
		 EQOS_DMA_SYSBUS_MODE_EAME | EQOS_DMA_SYSBUS_MODE_BLEN16 |
		 EQOS_DMA_SYSBUS_MODE_BLEN8 | EQOS_DMA_SYSBUS_MODE_BLEN4 |
		 EQOS_DMA_SYSBUS_MODE_FB;
	writel(val, &eqos->dma_regs->sysbus_mode);

	/* Set up descriptors */

	memset(eqos->descs, 0, EQOS_DESCRIPTORS_SIZE);
	for (i = 0; i < EQOS_DESCRIPTORS_RX; i++) {
		struct eqos_desc *rx_desc = &eqos->rx_descs[i];

		rx_desc->des0 = (u32)(ulong)(eqos->rx_dma_buf +
					     (i * EQOS_MAX_PACKET_SIZE));
		rx_desc->des3 |= EQOS_DESC3_OWN | EQOS_DESC3_BUF1V;
	}
	flush_cache((unsigned long)eqos->descs, EQOS_DESCRIPTORS_SIZE);

	writel(0, &eqos->dma_regs->ch0_txdesc_list_haddress);
	writel((uint)eqos->tx_descs, &eqos->dma_regs->ch0_txdesc_list_address);
	writel(EQOS_DESCRIPTORS_TX - 1,
	       &eqos->dma_regs->ch0_txdesc_ring_length);

	writel(0, &eqos->dma_regs->ch0_rxdesc_list_haddress);
	writel((uint)eqos->rx_descs, &eqos->dma_regs->ch0_rxdesc_list_address);
	writel(EQOS_DESCRIPTORS_RX - 1,
	       &eqos->dma_regs->ch0_rxdesc_ring_length);

	/* Recv All if necessary */
	val = EQOS_MAC_PACKET_FILTER_RA | EQOS_MAC_PACKET_FILTER_PROMODE;
	setbits_le32(&eqos->mac_regs->filter, val);

	/* Enable everything */

	setbits_le32(&eqos->mac_regs->configuration,
		     EQOS_MAC_CONFIGURATION_TE | EQOS_MAC_CONFIGURATION_RE);

	setbits_le32(&eqos->dma_regs->ch0_tx_control,
		     EQOS_DMA_CH0_TX_CONTROL_ST);
	setbits_le32(&eqos->dma_regs->ch0_rx_control,
		     EQOS_DMA_CH0_RX_CONTROL_SR);

	/* TX tail pointer not written until we need to TX a packet */
	/* Point RX tail pointer at last descriptor. Ideally, we'd point */
	/* at the first descriptor, implying all descriptors were available. */
	/* However, that's not distinguishable from none of the */
	/* descriptors being available. */
	last_rx_desc = (ulong)&eqos->rx_descs[(EQOS_DESCRIPTORS_RX - 1)];
	writel(last_rx_desc, &eqos->dma_regs->ch0_rxdesc_tail_pointer);

	eqos->started = true;

	//dcache_enable();

	debug("%s: OK\n", __func__);
	return 0;

err_shutdown_phy:
	phy_shutdown(eqos->phy);
	eqos->phy = NULL;
err_stop_resets:
	eqos_stop_resets(dev);
err_stop_clks:
	eqos_stop_clks(dev);
err:
	pr_err("FAILED: %d", ret);
	//dcache_enable();
	return ret;
}

void eqos_stop(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int i;

	debug("%s(dev=%p):\n", __func__, dev);

	if (!eqos->started)
		return;
	eqos->started = false;
	eqos->reg_access_ok = false;

	/* Disable TX DMA */
	clrbits_le32(&eqos->dma_regs->ch0_tx_control,
		     EQOS_DMA_CH0_TX_CONTROL_ST);

	/* Wait for TX all packets to drain out of MTL */
	for (i = 0; i < 10000; i++) {
		u32 val = readl(&eqos->mtl_regs->txq0_debug);
		u32 trcsts = (val >> EQOS_MTL_TXQ0_DEBUG_TRCSTS_SHIFT) &
			EQOS_MTL_TXQ0_DEBUG_TRCSTS_MASK;
		u32 txqsts = val & EQOS_MTL_TXQ0_DEBUG_TXQSTS;

		if (trcsts != 1 && !txqsts)
			break;
	}

	/* Turn off MAC TX and RX */
	clrbits_le32(&eqos->mac_regs->configuration,
		     EQOS_MAC_CONFIGURATION_TE | EQOS_MAC_CONFIGURATION_RE);

	/* Wait for all RX packets to drain out of MTL */
	for (i = 0; i < 10000; i++) {
		u32 val = readl(&eqos->mtl_regs->rxq0_debug);
		u32 prxq = (val >> EQOS_MTL_RXQ0_DEBUG_PRXQ_SHIFT) &
			EQOS_MTL_RXQ0_DEBUG_PRXQ_MASK;
		u32 rxqsts = (val >> EQOS_MTL_RXQ0_DEBUG_RXQSTS_SHIFT) &
			EQOS_MTL_RXQ0_DEBUG_RXQSTS_MASK;
		if (!prxq && !rxqsts)
			break;
	}

	/* Turn off RX DMA */
	clrbits_le32(&eqos->dma_regs->ch0_rx_control,
		     EQOS_DMA_CH0_RX_CONTROL_SR);

	if (eqos->phy) {
		phy_shutdown(eqos->phy);
		eqos->phy = NULL;
	}
	eqos_stop_resets(dev);
	eqos_stop_clks(dev);

	debug("%s: OK\n", __func__);
}

int eqos_send(struct udevice *dev, void *packet, int length)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct eqos_desc *tx_desc;
	int i;

	debug("%s(dev=%p, packet=%p, length=%d):\n", __func__, dev, packet,
	      length);
	//dcache_disable();

	memcpy(eqos->tx_dma_buf, packet, length);
	eqos_flush_buffer(eqos->tx_dma_buf, length);

	tx_desc = &eqos->tx_descs[eqos->tx_desc_idx];
	eqos->tx_desc_idx++;
	eqos->tx_desc_idx %= EQOS_DESCRIPTORS_TX;

	tx_desc->des0 = (ulong)eqos->tx_dma_buf;
	tx_desc->des1 = 0;
	tx_desc->des2 = length;
	/* Make sure that if HW sees the _OWN write below, it will see */
	/* all the writes to the rest of the descriptor too. */
	mb();
	tx_desc->des3 = EQOS_DESC3_OWN | EQOS_DESC3_FD | EQOS_DESC3_LD | length;
	eqos_flush_desc(tx_desc);

	writel((ulong)(tx_desc + 1), &eqos->dma_regs->ch0_txdesc_tail_pointer);

	for (i = 0; i < 10000; i++) {
		eqos_inval_desc(tx_desc);
		if (!(readl(&tx_desc->des3) & EQOS_DESC3_OWN)) {
			//dcache_enable();
			return 0;
		}
		udelay(1);
	}

	//dcache_enable();
	debug("%s: TX timeout\n", __func__);

	return -ETIMEDOUT;
}

int eqos_recv(struct udevice *dev, int flags, uchar **packetp)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	struct eqos_desc *rx_desc;
	int length;

	debug("%s(dev=%p, flags=%x):\n", __func__, dev, flags);
	//dcache_disable();
	rx_desc = &eqos->rx_descs[eqos->rx_desc_idx];
	if (rx_desc->des3 & EQOS_DESC3_OWN) {
		debug("%s: RX desc not avail 0x%p\n", __func__, rx_desc);
		//dcache_enable();
		return -EAGAIN;
	}

	*packetp = eqos->rx_dma_buf +
		(eqos->rx_desc_idx * EQOS_MAX_PACKET_SIZE);
	length = rx_desc->des3 & 0x7fff;
	debug("%s: *packetp=%p, length=%d\n", __func__, *packetp, length);

	eqos_inval_buffer(*packetp, length);
	//dcache_enable();
	return length;
}

int eqos_free_pkt(struct udevice *dev, uchar *packet, int length)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	uchar *packet_expected;
	struct eqos_desc *rx_desc;

	debug("%s(packet=%p, length=%d)\n", __func__, packet, length);

	packet_expected = eqos->rx_dma_buf +
		(eqos->rx_desc_idx * EQOS_MAX_PACKET_SIZE);
	if (packet != packet_expected) {
		debug("%s: Unexpected packet (expected %p)\n", __func__,
		      packet_expected);
		return -EINVAL;
	}

	rx_desc = &eqos->rx_descs[eqos->rx_desc_idx];
	rx_desc->des0 = (u32)(ulong)packet;
	rx_desc->des1 = 0;
	rx_desc->des2 = 0;
	/* Make sure that if HW sees the _OWN write below, it will see */
	/* all the writes to the rest of the descriptor too. */
	mb();
	rx_desc->des3 |= EQOS_DESC3_OWN | EQOS_DESC3_BUF1V;
	eqos_flush_desc(rx_desc);

	writel((ulong)rx_desc, &eqos->dma_regs->ch0_rxdesc_tail_pointer);

	eqos->rx_desc_idx++;
	eqos->rx_desc_idx %= EQOS_DESCRIPTORS_RX;

	return 0;
}

static int eqos_probe_resources_core(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	eqos->descs = eqos_alloc_descs(EQOS_DESCRIPTORS_TX +
				       EQOS_DESCRIPTORS_RX);
	if (!eqos->descs) {
		debug("%s: eqos_alloc_descs() failed\n", __func__);
		ret = -ENOMEM;
		goto err;
	}
	eqos->tx_descs = (struct eqos_desc *)eqos->descs;
	eqos->rx_descs = (eqos->tx_descs + EQOS_DESCRIPTORS_TX);
	debug("tx=0x%p, rx=0x%p\n", eqos->tx_descs, eqos->rx_descs);

	eqos->tx_dma_buf = memalign(EQOS_BUFFER_ALIGN, EQOS_MAX_PACKET_SIZE);
	if (!eqos->tx_dma_buf) {
		debug("%s: memalign(tx_dma_buf) failed\n", __func__);
		ret = -ENOMEM;
		goto err_free_descs;
	}
	debug("%s: tx_dma_buf=0x%p\n", __func__, eqos->tx_dma_buf);

	eqos->rx_dma_buf = memalign(EQOS_BUFFER_ALIGN, EQOS_RX_BUFFER_SIZE);
	if (!eqos->rx_dma_buf) {
		debug("%s: memalign(rx_dma_buf) failed\n", __func__);
		ret = -ENOMEM;
		goto err_free_tx_dma_buf;
	}
	debug("%s: rx_dma_buf=0x%p\n", __func__, eqos->rx_dma_buf);

	eqos->rx_pkt = malloc(EQOS_MAX_PACKET_SIZE);
	if (!eqos->rx_pkt) {
		debug("%s: malloc(rx_pkt) failed\n", __func__);
		ret = -ENOMEM;
		goto err_free_rx_dma_buf;
	}
	debug("%s: rx_pkt=%p\n", __func__, eqos->rx_pkt);

	debug("%s: OK\n", __func__);
	return 0;

err_free_rx_dma_buf:
	free(eqos->rx_dma_buf);
err_free_tx_dma_buf:
	free(eqos->tx_dma_buf);
err_free_descs:
	eqos_free_descs(eqos->descs);
err:

	debug("%s: returns %d\n", __func__, ret);
	return ret;
}

static int eqos_remove_resources_core(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	free(eqos->rx_pkt);
	free(eqos->rx_dma_buf);
	free(eqos->tx_dma_buf);
	eqos_free_descs(eqos->descs);

	debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_probe_resources(struct udevice *dev)
{
	const char *phy_mode;
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	eqos->phy_interface = PHY_INTERFACE_MODE_NONE;
	phy_mode = dev_read_string(dev, "phy-mode");
	if (phy_mode)
		eqos->phy_interface = phy_get_interface_by_name(phy_mode);

	if (eqos->phy_interface == PHY_INTERFACE_MODE_NONE) {
		pr_err("Get phy-mode failed\n");
		return -EBUSY;
	}

	ret = get_board_type();
	if (ret < 0)
		return -ENXIO;

	if (ret == BSTA1000B_FADA)
		eqos->phy_addr = 1;
	else if (ret == BSTA1000B_FADB)
		eqos->phy_addr = 2;
	else
		eqos->phy_addr = 0;
	eqos->board = ret;

	ret = gpio_request_by_name(dev, "phy-reset-gpios",
				   0, &eqos->phy_reset_gpio, 0);
	if (ret) {
		pr_err("gpio_request_by_name(phy reset) failed: %d", ret);
		return -EBUSY;
	}

	if (dm_gpio_is_valid(&eqos->phy_reset_gpio))
		dm_gpio_set_dir_flags(&eqos->phy_reset_gpio,
				      GPIOD_IS_OUT | GPIOD_IS_OUT_ACTIVE);

#if UBOOT_CLK_SET
	ret = clk_get_by_name(dev, "slave_bus", &eqos->clk_slave_bus);
	if (ret) {
		pr_err("clk_get_by_name(slave_bus) failed: %d", ret);
		goto err_free_gpio_phy_reset;
	}

	ret = clk_get_by_name(dev, "master_bus", &eqos->clk_master_bus);
	if (ret) {
		pr_err("clk_get_by_name(master_bus) failed: %d", ret);
		goto err_free_clk_slave_bus;
	}

	ret = clk_get_by_name(dev, "rx", &eqos->clk_rx);
	if (ret) {
		pr_err("clk_get_by_name(rx) failed: %d", ret);
		goto err_free_clk_master_bus;
	}

	ret = clk_get_by_name(dev, "ptp_ref", &eqos->clk_ptp_ref);
	if (ret) {
		pr_err("clk_get_by_name(ptp_ref) failed: %d", ret);
		goto err_free_clk_rx;
		return ret;
	}

	ret = clk_get_by_name(dev, "tx", &eqos->clk_tx);
	if (ret) {
		pr_err("clk_get_by_name(tx) failed: %d", ret);
		goto err_free_clk_ptp_ref;
	}

	ret = reset_get_by_name(dev, "resets", &eqos->reset_ctl);
	if (ret) {
		pr_err("reset_get_by_name(rst) failed: %d", ret);
		goto err_free_clk_tx;
	}
#endif
	debug("%s: OK\n", __func__);
	return 0;
#if UBOOT_CLK_SET
err_free_clk_tx:
//	clk_free(&eqos->clk_tx);
err_free_clk_ptp_ref:
//	clk_free(&eqos->clk_ptp_ref);
err_free_clk_rx:
//	clk_free(&eqos->clk_rx);
err_free_clk_master_bus:
//	clk_free(&eqos->clk_master_bus);
err_free_clk_slave_bus:
//	clk_free(&eqos->clk_slave_bus);
err_free_gpio_phy_reset:
//  dm_gpio_free(dev, &eqos->phy_reset_gpio);
#endif
	debug("%s: returns %d\n", __func__, ret);
	return ret;
}

static int eqos_remove_resources(struct udevice *dev)
{
//	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

//	clk_free(&eqos->clk_tx);
//	clk_free(&eqos->clk_ptp_ref);
//	clk_free(&eqos->clk_rx);
//	clk_free(&eqos->clk_slave_bus);
//	clk_free(&eqos->clk_master_bus);
//	dm_gpio_free(dev, &eqos->phy_reset_gpio);
//	reset_free(&eqos->reset_ctl);

	debug("%s: OK\n", __func__);
	return 0;
}

static int eqos_probe(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);
	int ret;

	debug("%s(dev=%p):\n", __func__, dev);

	writel(0x3, 0x33000054);//rgmii

	eqos->dev = dev;
	eqos->config = (void *)dev_get_driver_data(dev);

	eqos->regs = devfdt_get_addr(dev);
	if (eqos->regs == FDT_ADDR_T_NONE) {
		pr_err("devfdt_get_addr() failed");
		return -ENODEV;
	}
	eqos->mac_regs = (void *)(eqos->regs + EQOS_MAC_REGS_BASE);
	eqos->mtl_regs = (void *)(eqos->regs + EQOS_MTL_REGS_BASE);
	eqos->dma_regs = (void *)(eqos->regs + EQOS_DMA_REGS_BASE);

	ret = eqos_probe_resources_core(dev);
	if (ret < 0) {
		pr_err("eqos_probe_resources_core() failed: %d", ret);
		return ret;
	}

	ret = eqos_probe_resources(dev);
	if (ret < 0) {
		pr_err("eqos_probe_resources() failed: %d", ret);
		goto err_remove_resources_core;
	}

	eqos->mii = mdio_alloc();
	if (!eqos->mii) {
		pr_err("mdio_alloc() failed");
		goto err_remove_resources_tegra;
	}
	eqos->mii->read = eqos_mdio_read;
	eqos->mii->write = eqos_mdio_write;
	eqos->mii->priv = eqos;
	strcpy(eqos->mii->name, dev->name);

	ret = mdio_register(eqos->mii);
	if (ret < 0) {
		pr_err("mdio_register() failed: %d", ret);
		goto err_free_mdio;
	}

	debug("%s: OK\n", __func__);
	return 0;

err_free_mdio:
	mdio_free(eqos->mii);
err_remove_resources_tegra:
	eqos_remove_resources(dev);
err_remove_resources_core:
	eqos_remove_resources_core(dev);

	debug("%s: returns %d\n", __func__, ret);
	return ret;
}

static int eqos_remove(struct udevice *dev)
{
	struct eqos_priv *eqos = dev_get_priv(dev);

	debug("%s(dev=%p):\n", __func__, dev);

	mdio_unregister(eqos->mii);
	mdio_free(eqos->mii);
	eqos_remove_resources(dev);
	eqos_probe_resources_core(dev);

	debug("%s: OK\n", __func__);
	return 0;
}

static const struct eth_ops eqos_ops = {
	.start = eqos_start,
	.stop = eqos_stop,
	.send = eqos_send,
	.recv = eqos_recv,
	.free_pkt = eqos_free_pkt,
	.write_hwaddr = eqos_write_hwaddr,
};

static const struct eqos_config eqos_config = {
	.reg_access_always_ok = false,
};

static const struct udevice_id eqos_ids[] = {
	{
		.compatible = "bst,a1000-eth-eqos",
		.data = (ulong)&eqos_config
	},
	{ }
};

U_BOOT_DRIVER(eth_eqos) = {
	.name = "eth_eqos",
	.id = UCLASS_ETH,
	.of_match = eqos_ids,
	.probe = eqos_probe,
	.remove = eqos_remove,
	.ops = &eqos_ops,
	.priv_auto_alloc_size = sizeof(struct eqos_priv),
	.platdata_auto_alloc_size = sizeof(struct eth_pdata),
};
