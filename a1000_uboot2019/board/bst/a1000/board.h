/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * (C) Copyright 2020 BlackSesame Tec Ltd.
 */
#ifndef __BST_A1000_BOARD_H_
#define __BST_A1000_BOARD_H_

#include <linux/bitops.h>

#define A1000BASE_TOPCRM (0x33002000UL)

/* QSPI Reset */
#define A1000BASE_SAFETYCRM 0x70035000
#define A1000BASE_AONCFG 0x70038000
#define A1000BASE_QSPI0 0x00000000
#define A1000BASE_LBLSP0 (0x20020000UL)
#define A1000BASE_LBLSP1 (0x20021000UL)
#define CPU_CSR_BASE 0x32011000

#define CPU_CSR_CORE_CONFIG_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x00))
#define CPU_CSR_RVBARADDR1_REG (*(volatile unsigned int *)(CPU_CSR_BASE + 0x04))
#define CPU_CSR_RVBARADDR2_REG (*(volatile unsigned int *)(CPU_CSR_BASE + 0x08))
#define CPU_CSR_RVBARADDR3_REG (*(volatile unsigned int *)(CPU_CSR_BASE + 0x0C))
#define CPU_CSR_RVBARADDR4_REG (*(volatile unsigned int *)(CPU_CSR_BASE + 0x10))
#define CPU_CSR_RVBARADDR5_REG (*(volatile unsigned int *)(CPU_CSR_BASE + 0x14))
#define CPU_CSR_RVBARADDR6_REG (*(volatile unsigned int *)(CPU_CSR_BASE + 0x18))
#define CPU_CSR_RVBARADDR7_REG (*(volatile unsigned int *)(CPU_CSR_BASE + 0x1C))
#define CPU_CSR_CORE0_RESET_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x20))
#define CPU_CSR_CORE1_RESET_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x24))
#define CPU_CSR_CORE2_RESET_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x28))
#define CPU_CSR_CORE3_RESET_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x2C))
#define CPU_CSR_CORE4_RESET_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x30))
#define CPU_CSR_CORE5_RESET_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x34))
#define CPU_CSR_CORE6_RESET_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x38))
#define CPU_CSR_CORE7_RESET_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x3C))
#define CPU_CSR_CORE0_CLKEN_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x40))
#define CPU_CSR_CORE1_CLKEN_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x44))
#define CPU_CSR_CORE2_CLKEN_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x48))
#define CPU_CSR_CORE3_CLKEN_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x4C))
#define CPU_CSR_CORE4_CLKEN_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x50))
#define CPU_CSR_CORE5_CLKEN_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x54))
#define CPU_CSR_CORE6_CLKEN_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x58))
#define CPU_CSR_CORE7_CLKEN_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x5C))
#define CPU_CSR_PERIPH_DEBG_RESET_REG \
	(*(volatile unsigned int *)(CPU_CSR_BASE + 0x60))

/* pll config0 item */
#define PLL_PLLEN (5)
/* pll(gmac) special item */
#define PLL_GMAC_LOCK (1)
#define FREQ_CRYSTAL (25 * 1000000UL)
#define FOUT_PLL_GMAC (1000 * 1000000UL)
#define A1000BASE_PLL_GMAC (A1000BASE_TOPCRM + 0x8C)
#define TOPCRM_REG_R_CLKMUX_SEL0 (A1000BASE_TOPCRM + 0x9C)
#define TOPCRM_REG_R_CLKMUX_SEL1 (A1000BASE_TOPCRM + 0xA0)
#define TOPCRM_REG_R_CLKGATE_EN0 (A1000BASE_TOPCRM + 0xA8)
#define A1000BASE_SYSCTRL (0x33000000UL)
#define A1000BASE_TOPCRM (0x33002000UL)
#define A1000BASE_PMMREG (0x33001000UL)

#define A1000BASE_TOPCRM_RST0 (A1000BASE_TOPCRM + 0x4)
#define TOPCRM_REG_R_PLL_CLKMUX_SEL (A1000BASE_TOPCRM + 0xA4)

extern ulong _TEXT_SPL_BASE;

#define BST_DDR_INFO_ADDR (0x120000)
#ifdef BST_DDR_INFO_ADDR
#define DDR_INFO_SIZE (0x10000)
#define DDR_NAME_LEN (32)

typedef enum {
	INTERLEAVE_OPEN = 0,
	INTERLEAVE_CLOSE,
	DDR_INTERLEAVE_END,
} bool_ddr_interleave;

typedef enum {
	ECC_OPEN = 0,
	ECC_CLOSE,
	DDR_ECC_END,
} bool_ddr_ECC;

typedef enum {
	SAMSUNG = 0,
	MICRON,
	HYNIX,
	OTHERS,
	DDR_VENDER_NAME_END,
} ddr_Vender_Name;

typedef enum {
	DDR_FREQ_3200 = 0,
	DDR_FREQ_3200_2D,
	DDR_FREQ_2667,
	DDR_FREQ_2667_2D,
	DDR_FREQ_2133,
	DDR_FREQ_1600,
	DDR_FREQ_1066,
	DDR_FREQ_3732,
	DDR_FREQ_4267,
	DDR_FREQ_3732_2D,
	DDR_FREQ_4267_2D,
	DDR_FREQ_4000_2D,
	DDR_FREQ_4000,
	DDR_FREQ_END,
} ddr_Frequency;

/* config for ddr info */
typedef struct {
	unsigned int ddrVender;
	unsigned int ddrFre;
	unsigned int ddrInterleave;
	unsigned int ddrECC;
	/*contain partiy occap reg_parity aix,0- open; 1-close*/
	unsigned int ddrOTHERS;
	/*0-open; 1-close*/
	unsigned int clksscg;
	/* 0x0-none;1-only ddr0; 0x2-ddr0 & ddr1;*/
	unsigned int ddr_init_count;
	/*A1000B evb is 2 rank[2], A1000 EVB IS 1 RANK[1]*/
	unsigned int rank_count;
	/*signature verification:0 open, 1 close*/
	unsigned int sha_veri;
	/*0-xip_mode, 1-dma_mode*/
	unsigned int qspi_mode;
	/*ecc_range 0x7F=all open ecc; 0x01=only first 1/8 open ecc*/
	unsigned int ecc_range;
	/*0-OPEN, 1-CLOSE; swappin: only for oflim 1 rank board*/
	unsigned int swappin;
	/*CHIP_ID, USED FOR PLL CONFIG*/
	unsigned int chip_id;
	/*0:ZONA; 1:ZONB*/
	unsigned int zone;
	/*0x5a5a1234 is adaptaed, others is not adapted*/
	unsigned int adapte;
	unsigned int reserved7;
	unsigned int reserved8;
	unsigned int reserved9;
} T_CFG_DDR;

typedef struct __s_ddr_config {
	unsigned int ddr_vender;
	unsigned int ddr_fre;
	unsigned int ddr_firmware_size;
	char chip_id[16]; /*A1000 OR A1000B*/
	char ddr_id[16];
	char ddr_config_hashVal[64];
	char ddr_para_1d_hashVal[64];
	char ddr_para_2d_hashVal[64];
} s_ddr_config;

#define DDR_FIRMWARE_start (0x1c0000)
#define DDR_FIRMWARE_MAX_SIZE (0x20000)
#define DDR_FIRMWARE_3200_2D (DDR_FIRMWARE_start)
#define DDR_FIRMWARE_3200 (DDR_FIRMWARE_3200_2D + SZ_64K)

#endif // BST_DDR_INFO_ADDR

int get_bootmode(void);
void clk_init(void);
void bst_close_xip(int index, u32 clkdiv);
u32 bst_open_xip(int index, u32 size);
void init_qspi(void);
int board_init(void);
void *nvram_read(void *dest, const long src, size_t count);
void nvram_write(long dest, const void *src, size_t count);

#endif
