// SPDX-License-Identifier: GPL-2.0+
/**
 * Broadcom PHY drivers
 *
 * Copyright 2010-2011 Freescale Semiconductor, Inc.
 * author Andy Fleming
 */
#include <common.h>
#include <phy.h>

/* Broadcom BCM54xx -- taken from linux sungem_phy */
#define MIIM_BCM54xx_AUXCNTL			0x18
#define MIIM_BCM54xx_AUXCNTL_ENCODE(val) (((val & 0x7) << 12) | (val & 0x7))
#define MIIM_BCM54xx_AUXSTATUS			0x19
#define MIIM_BCM54xx_AUXSTATUS_LINKMODE_MASK	0x0700
#define MIIM_BCM54xx_AUXSTATUS_LINKMODE_SHIFT	8

#define MIIM_BCM54XX_SHD			0x1c
#define MIIM_BCM54XX_SHD_WRITE			0x8000
#define MIIM_BCM54XX_SHD_VAL(x)			((x & 0x1f) << 10)
#define MIIM_BCM54XX_SHD_DATA(x)		((x & 0x3ff) << 0)
#define MIIM_BCM54XX_SHD_WR_ENCODE(val, data)	\
	(MIIM_BCM54XX_SHD_WRITE | MIIM_BCM54XX_SHD_VAL(val) | \
	 MIIM_BCM54XX_SHD_DATA(data))

#define MIIM_BCM54XX_EXP_DATA		0x15	/* Expansion register data */
#define MIIM_BCM54XX_EXP_SEL		0x17	/* Expansion register select */
#define MIIM_BCM54XX_EXP_SEL_SSD	0x0e00	/* Secondary SerDes select */
#define MIIM_BCM54XX_EXP_SEL_ER		0x0f00	/* Expansion register select */

#define MIIM_BCM_AUXCNTL_SHDWSEL_MISC	0x0007
#define MIIM_BCM_AUXCNTL_ACTL_SMDSP_EN	0x0800

#define MIIM_BCM_CHANNEL_WIDTH    0x2000

static void bcm_phy_write_misc(struct phy_device *phydev,
			       u16 reg, u16 chl, u16 value)
{
	int reg_val;

	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54xx_AUXCNTL,
		  MIIM_BCM_AUXCNTL_SHDWSEL_MISC);

	reg_val = phy_read(phydev, MDIO_DEVAD_NONE, MIIM_BCM54xx_AUXCNTL);
	reg_val |= MIIM_BCM_AUXCNTL_ACTL_SMDSP_EN;
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54xx_AUXCNTL, reg_val);

	reg_val = (chl * MIIM_BCM_CHANNEL_WIDTH) | reg;
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_EXP_SEL, reg_val);

	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_EXP_DATA, value);
}

/* Broadcom BCM5461S */
static int bcm5461_config(struct phy_device *phydev)
{
	genphy_config_aneg(phydev);

	phy_reset(phydev);

	return 0;
}

static int bcm54xx_parse_status(struct phy_device *phydev)
{
	unsigned int mii_reg;

	mii_reg = phy_read(phydev, MDIO_DEVAD_NONE, MIIM_BCM54xx_AUXSTATUS);

	switch ((mii_reg & MIIM_BCM54xx_AUXSTATUS_LINKMODE_MASK) >>
			MIIM_BCM54xx_AUXSTATUS_LINKMODE_SHIFT) {
	case 1:
		phydev->duplex = DUPLEX_HALF;
		phydev->speed = SPEED_10;
		break;
	case 2:
		phydev->duplex = DUPLEX_FULL;
		phydev->speed = SPEED_10;
		break;
	case 3:
		phydev->duplex = DUPLEX_HALF;
		phydev->speed = SPEED_100;
		break;
	case 5:
		phydev->duplex = DUPLEX_FULL;
		phydev->speed = SPEED_100;
		break;
	case 6:
		phydev->duplex = DUPLEX_HALF;
		phydev->speed = SPEED_1000;
		break;
	case 7:
		phydev->duplex = DUPLEX_FULL;
		phydev->speed = SPEED_1000;
		break;
	default:
		printf("Auto-neg error, defaulting to 10BT/HD\n");
		phydev->duplex = DUPLEX_HALF;
		phydev->speed = SPEED_10;
		break;
	}

	return 0;
}

static int bcm54xx_startup(struct phy_device *phydev)
{
	int ret;

	/* Read the Status (2x to make sure link is right) */
	ret = genphy_update_link(phydev);
	if (ret)
		return ret;

	return bcm54xx_parse_status(phydev);
}

/* Broadcom BCM5482S */
/**
 * "Ethernet@Wirespeed" needs to be enabled to achieve link in certain
 * circumstances.  eg a gigabit TSEC connected to a gigabit switch with
 * a 4-wire ethernet cable.  Both ends advertise gigabit, but can't
 * link.  "Ethernet@Wirespeed" reduces advertised speed until link
 * can be achieved.
 */
static u32 bcm5482_read_wirespeed(struct phy_device *phydev, u32 reg)
{
	return (phy_read(phydev, MDIO_DEVAD_NONE, reg) & 0x8FFF) | 0x8010;
}

static int bcm5482_config(struct phy_device *phydev)
{
	unsigned int reg;

	/* reset the PHY */
	reg = phy_read(phydev, MDIO_DEVAD_NONE, MII_BMCR);
	reg |= BMCR_RESET;
	phy_write(phydev, MDIO_DEVAD_NONE, MII_BMCR, reg);

	/* Setup read from auxilary control shadow register 7 */
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54xx_AUXCNTL,
		  MIIM_BCM54xx_AUXCNTL_ENCODE(7));
	/* Read Misc Control register and or in Ethernet@Wirespeed */
	reg = bcm5482_read_wirespeed(phydev, MIIM_BCM54xx_AUXCNTL);
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54xx_AUXCNTL, reg);

	/* Initial config/enable of secondary SerDes interface */
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_SHD,
		  MIIM_BCM54XX_SHD_WR_ENCODE(0x14, 0xf));
	/* Write initial value to secondary SerDes Control */
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_EXP_SEL,
		  MIIM_BCM54XX_EXP_SEL_SSD | 0);
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_EXP_DATA,
		  BMCR_ANRESTART);
	/* Enable copper/fiber auto-detect */
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_SHD,
		  MIIM_BCM54XX_SHD_WR_ENCODE(0x1e, 0x201));

	genphy_config_aneg(phydev);

	return 0;
}

static int bcm_cygnus_startup(struct phy_device *phydev)
{
	int ret;

	/* Read the Status (2x to make sure link is right) */
	ret = genphy_update_link(phydev);
	if (ret)
		return ret;

	return genphy_parse_link(phydev);
}

static void bcm_cygnus_afe(struct phy_device *phydev)
{
	/* ensures smdspclk is enabled */
	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54xx_AUXCNTL, 0x0c30);

	/* AFE_VDAC_ICTRL_0 bit 7:4 Iq=1100 for 1g 10bt, normal modes */
	bcm_phy_write_misc(phydev, 0x39, 0x01, 0xA7C8);

	/* AFE_HPF_TRIM_OTHERS bit11=1, short cascode for all modes*/
	bcm_phy_write_misc(phydev, 0x3A, 0x00, 0x0803);

	/* AFE_TX_CONFIG_1 bit 7:4 Iq=1100 for test modes */
	bcm_phy_write_misc(phydev, 0x3A, 0x01, 0xA740);

	/* AFE TEMPSEN_OTHERS rcal_HT, rcal_LT 10000 */
	bcm_phy_write_misc(phydev, 0x3A, 0x03, 0x8400);

	/* AFE_FUTURE_RSV bit 2:0 rccal <2:0>=100 */
	bcm_phy_write_misc(phydev, 0x3B, 0x00, 0x0004);

	/* Adjust bias current trim to overcome digital offSet */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x1E, 0x02);

	/* make rcal=100, since rdb default is 000 */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x17, 0x00B1);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x15, 0x0010);

	/* CORE_EXPB0, Reset R_CAL/RC_CAL Engine */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x17, 0x00B0);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x15, 0x0010);

	/* CORE_EXPB0, Disable Reset R_CAL/RC_CAL Engine */
	phy_write(phydev, MDIO_DEVAD_NONE, 0x17, 0x00B0);
	phy_write(phydev, MDIO_DEVAD_NONE, 0x15, 0x0000);
}

static int bcm_cygnus_config(struct phy_device *phydev)
{
	genphy_config_aneg(phydev);
	phy_reset(phydev);
	/* AFE settings for PHY stability */
	bcm_cygnus_afe(phydev);
	/* Forcing aneg after applying the AFE settings */
	genphy_restart_aneg(phydev);

	return 0;
}

#define BST_GMAC1_DEVAD 0x1
#define BST_GMAC1_PHY_TIMEOUT 5000

int bst_89x_miiphy_read(struct phy_device *phy, int devad,
			unsigned char reg, unsigned short *value)
{
	struct mii_dev *bus = phy->bus;
	int ret;

	ret = 0;
	if (bus) {
		ret = bus->read(bus, phy->addr, devad, reg);
		if (ret < 0)
			return 1;
	}
	*value = ret & 0xffff;
	return 0;
}

/*****************************************************************************
 *
 * Write <value> to the PHY attached to device <devname>,
 * use PHY address <addr> and register <reg>.
 *
 * This API is deprecated. Use phy_write on a phy_device found by phy_connect
 *
 * Returns:
 *   0 on success
 */
int bst_89x_miiphy_write(struct phy_device *phy, int devad,
			 unsigned char reg, unsigned short value)
{
	struct mii_dev *bus = phy->bus;

	if (bus)
		return bus->write(bus, phy->addr, devad, reg, value);

	return 1;
}

int bst_89x_waitfor_link(struct phy_device *phy)
{
	unsigned short value = 0;
	int cnt = 0;

	printf("Waiting for PHY Link to complete");

	do {
		bst_89x_miiphy_read(phy, BST_GMAC1_DEVAD, 1, &value);
		if (value & 1 << 2) {
			phy->link = 1;
			printf("OK\n\r");
			return 0;
		}
		if (cnt >= BST_GMAC1_PHY_TIMEOUT / 100) {
			phy->link = 0;
			printf("TimeOUT\n\r");
			return -1;
		}
		cnt++;
		if (!(cnt & 0x7))
			printf(".");
		udelay(100 * 1000);
	} while (1);
}

int bst_89x_master_init(struct phy_device *phy)
{
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x00, 0x8040);
	udelay(1000 * 1000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x00, 0x40);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x0B, 0x0000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x1D, 0x3410);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x1E, 0x3863);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x27, 0x0317);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x19, 0x2508);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x00, 0xe2e1);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x01, 0x0002);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x02, 0x0001);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x03, 0x0006);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x04, 0x0005);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x05, 0x9fbe);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x06, 0x25bf);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x10, 0x7a7b);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x11, 0x8f80);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x12, 0x8000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x13, 0x0006);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x14, 0x0001);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x15, 0x8fbe);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x16, 0x25bf);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x20, 0x0095);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x21, 0x8007);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x22, 0x0000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x23, 0x0406);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x24, 0x0001);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x25, 0x8fbe);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x26, 0x25bf);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x30, 0xe005);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x31, 0x0079);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x32, 0x0094);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x34, 0xC001);
	udelay(100 * 1000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x10, 0x0101);
	udelay(100 * 1000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x10, 0x0101);
	bst_89x_miiphy_write(phy, 0x7, 0x00, 0x0200);

	return bst_89x_waitfor_link(phy);
}

int bst_89x_slave_init(struct phy_device *phy)
{
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x00, 0x8040);
	udelay(1000 * 1000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x00, 0x40);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x0B, 0x0000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x1D, 0x3410);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x1E, 0x3863);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x27, 0x0317);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x19, 0x2508);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x00, 0xe2e1);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x01, 0x0002);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x02, 0x0001);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x03, 0x0006);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x04, 0x0005);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x05, 0x9fbe);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x06, 0x25bf);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x10, 0x7a7b);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x11, 0x8f80);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x12, 0x8000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x13, 0x0006);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x14, 0x0001);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x15, 0x8fbe);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x16, 0x25bf);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x20, 0x0095);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x21, 0x8007);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x22, 0x0000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x23, 0x0406);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x24, 0x0001);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x25, 0x8fbe);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x26, 0x25bf);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x30, 0xe005);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x31, 0x0079);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x32, 0x0094);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x34, 0x8001);
	udelay(100 * 1000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x10, 0x0101);
	udelay(100 * 1000);
	bst_89x_miiphy_write(phy, BST_GMAC1_DEVAD, 0x10, 0x0101);
	bst_89x_miiphy_write(phy, 0x7, 0x00, 0x0200);

	return bst_89x_waitfor_link(phy);
}

static int bcm8988x_config(struct phy_device *phydev)
{
	char *wmodel = NULL;
	int work_mode = 1;

	wmodel = env_get("bcm_wmod");
	if ((wmodel) && (!strcmp(wmodel, "slave")))
		work_mode = 0;

	if (work_mode) {
		printf("Init bst_89x_master_init\n");
		bst_89x_master_init(phydev);
	} else {
		printf("Init bst_89x_slave_init\n");
		bst_89x_slave_init(phydev);
	}
	return 0;
}

static int bcm8988xx_parse_status(struct phy_device *phydev)
{
	phydev->duplex = DUPLEX_FULL;
	phydev->speed = SPEED_1000;

	return 0;
}

static int bcm8988x_startup(struct phy_device *phydev)
{
	int ret;

	/* Read the Status (2x to make sure link is right) */
	ret = bst_89x_waitfor_link(phydev);
	if (ret)
		return ret;

	return bcm8988xx_parse_status(phydev);
}

/**
 * Find out if PHY is in copper or serdes mode by looking at Expansion Reg
 * 0x42 - "Operating Mode Status Register"
 */
static int bcm5482_is_serdes(struct phy_device *phydev)
{
	u16 val;
	int serdes = 0;

	phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_EXP_SEL,
		  MIIM_BCM54XX_EXP_SEL_ER | 0x42);
	val = phy_read(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_EXP_DATA);

	switch (val & 0x1f) {
	case 0x0d:	/* RGMII-to-100Base-FX */
	case 0x0e:	/* RGMII-to-SGMII */
	case 0x0f:	/* RGMII-to-SerDes */
	case 0x12:	/* SGMII-to-SerDes */
	case 0x13:	/* SGMII-to-100Base-FX */
	case 0x16:	/* SerDes-to-Serdes */
		serdes = 1;
		break;
	case 0x6:	/* RGMII-to-Copper */
	case 0x14:	/* SGMII-to-Copper */
	case 0x17:	/* SerDes-to-Copper */
		break;
	default:
		printf("ERROR, invalid PHY mode (0x%x\n)", val);
		break;
	}

	return serdes;
}

/**
 * Determine SerDes link speed and duplex from Expansion reg 0x42 "Operating
 * Mode Status Register"
 */
static u32 bcm5482_parse_serdes_sr(struct phy_device *phydev)
{
	u16 val;
	int i = 0;

	/* Wait 1s for link - Clause 37 autonegotiation happens very fast */
	while (1) {
		phy_write(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_EXP_SEL,
			  MIIM_BCM54XX_EXP_SEL_ER | 0x42);
		val = phy_read(phydev, MDIO_DEVAD_NONE, MIIM_BCM54XX_EXP_DATA);

		if (val & 0x8000)
			break;

		if (i++ > 1000) {
			phydev->link = 0;
			return 1;
		}

		udelay(1000);	/* 1 ms */
	}

	phydev->link = 1;
	switch ((val >> 13) & 0x3) {
	case (0x00):
		phydev->speed = 10;
		break;
	case (0x01):
		phydev->speed = 100;
		break;
	case (0x02):
		phydev->speed = 1000;
		break;
	}

	phydev->duplex = (val & 0x1000) == 0x1000;

	return 0;
}

/**
 * Figure out if BCM5482 is in serdes or copper mode and determine link
 * configuration accordingly
 */
static int bcm5482_startup(struct phy_device *phydev)
{
	int ret;

	if (bcm5482_is_serdes(phydev)) {
		bcm5482_parse_serdes_sr(phydev);
		phydev->port = PORT_FIBRE;
		return 0;
	}

	/* Wait for auto-negotiation to complete or fail */
	ret = genphy_update_link(phydev);
	if (ret)
		return ret;

	/* Parse BCM54xx copper aux status register */
	return bcm54xx_parse_status(phydev);
}

static struct phy_driver BCM5461S_driver = {
	.name = "Broadcom BCM5461S",
	.uid = 0x2060c0,
	.mask = 0xfffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &bcm5461_config,
	.startup = &bcm54xx_startup,
	.shutdown = &genphy_shutdown,
};

static struct phy_driver BCM5464S_driver = {
	.name = "Broadcom BCM5464S",
	.uid = 0x2060b0,
	.mask = 0xfffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &bcm5461_config,
	.startup = &bcm54xx_startup,
	.shutdown = &genphy_shutdown,
};

static struct phy_driver BCM5482S_driver = {
	.name = "Broadcom BCM5482S",
	.uid = 0x143bcb0,
	.mask = 0xffffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &bcm5482_config,
	.startup = &bcm5482_startup,
	.shutdown = &genphy_shutdown,
};

static struct phy_driver BCM_CYGNUS_driver = {
	.name = "Broadcom CYGNUS GPHY",
	.uid = 0xae025200,
	.mask = 0xfffff0,
	.features = PHY_GBIT_FEATURES,
	.config = &bcm_cygnus_config,
	.startup = &bcm_cygnus_startup,
	.shutdown = &genphy_shutdown,
};

static struct phy_driver BCM89881S_driver = {
	.name = "Broadcom BCM89881",
	.uid = 0xae025030,
	.mask = 0xfffffc,
	.features = PHY_GBIT_FEATURES,
	.config = &bcm8988x_config,
	.startup = &bcm8988x_startup,
	.shutdown = &genphy_shutdown,
};

int phy_broadcom_init(void)
{
	phy_register(&BCM5482S_driver);
	phy_register(&BCM5464S_driver);
	phy_register(&BCM5461S_driver);
	phy_register(&BCM_CYGNUS_driver);
	phy_register(&BCM89881S_driver);

	return 0;
}
