// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2020 BlackSesame Tec Ltd.
 */
#include <common.h>
#include <asm/system.h>
#include <asm/types.h>
#include <asm/io.h>
#include <fdtdec.h>
#include <asm/sections.h>
#include <malloc.h>
#include <boot_fit.h>
#include <common.h>
#include <errno.h>
#include <image.h>
#include <linux/libfdt.h>
#include <environment.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <mmc.h>
#include <ext4fs.h>
#include <spi_flash.h>
#include <linux/arm-smccc.h>
#include <u-boot/sha256.h>
#include <asm/spin_table.h>

#include "board.h"

DECLARE_GLOBAL_DATA_PTR;

static void set_tbclk(unsigned long cntfrq)
{
	asm volatile("msr cntfrq_el0, %x0" : : "rZ"(cntfrq));
	isb();
}

static void bst_uboot_smc_reset(void)
{
	struct arm_smccc_res res;

	arm_smccc_smc(BST_UBOOT_SMC_RESET, 0, 0, 0, 0, 0, 0, 0, &res);
}

int timer_init(void)
{
#ifdef CONDFIG_SPL_BUILD
	/* after ATF is utilized the uboot is in EL2,
	 * and is not allowed to set cntfrq_el0.
	 * only allow SPL set this register.
	 */
	set_tbclk(gd->cpu_clk / 4);
#endif
	/* armv8 gtimer enable */
	writel(0x1, (void *)0x32707000);
	return 0;
}

int get_bootmode(void)
{
	return (readl(A1000BASE_TOPCRM) >> 26) & 0x7;
}

#if 1
extern void exit_4byte_qspi(void);
static void reset_qspi(void)
{
	u32 value;

	exit_4byte_qspi(); /* Exit 4-byte mode, do reset flash */
	//do QSPI reset
	value = readl(A1000BASE_SAFETYCRM + 0x8);
	writel(value & (~(3 << 15)), A1000BASE_SAFETYCRM + 0x8);
	mdelay(100);
	writel(value | (3 << 15), A1000BASE_SAFETYCRM + 0x8);
	mdelay(100);
}
#else
static void reset_qspi(void)
{
	u32 value;

	//set QSPI CS internal ways
	value = readl(A1000BASE_AONCFG + 0x4);
	writel(value & (~(1 << 29)), A1000BASE_AONCFG + 0x4);

	//do QSPI reset
	value = readl(A1000BASE_SAFETYCRM + 0x8);
	writel(value & (~(3 << 15)), A1000BASE_SAFETYCRM + 0x8);
	mdelay(100);
	writel(value | (3 << 15), A1000BASE_SAFETYCRM + 0x8);
	mdelay(100);

	//set xip set 3 byte mode
	writel(0, A1000BASE_QSPI0 + 0x8);
	value = readl(A1000BASE_QSPI0 + 0x108) & (0xffffff0f);
	writel(value | 0x60, A1000BASE_QSPI0 + 0x108);
	writel(1, A1000BASE_QSPI0 + 0x8);

	//open XIP
	value = readl(A1000BASE_LBLSP0 + 0x10);
	writel(value | (0x1), A1000BASE_LBLSP0 + 0x10);
	writel(value | (0x1), A1000BASE_LBLSP0 + 0x10);
	mdelay(100);
}
#endif

void reset_cpu(ulong addr)
{
	reset_qspi();
	bst_uboot_smc_reset();

	while (1)
		asm("nop");
}

void bst_close_xip(int index, u32 clkdiv)
{
	u32 value;

	if (clkdiv == 0) {
		if (index == 0) {
			value = readl(A1000BASE_LBLSP0 + 0x14);
			writel(value & ~BIT(0), A1000BASE_LBLSP0 + 0x14);
		} else {
			value = readl(A1000BASE_LBLSP0 + 0x14);
			writel(value & ~BIT(1), A1000BASE_LBLSP0 + 0x14);
		}
	} else {
		/* TODO index, Close XIP qspi0 */
		value = readl(A1000BASE_LBLSP0 + 0x14);
		writel(value & (~0x1), A1000BASE_LBLSP0 + 0x14);
		//do QSPI reset
		value = readl(A1000BASE_SAFETYCRM + 0x8);
		writel(value & (~(1 << 16)), A1000BASE_SAFETYCRM + 0x8);
		udelay(1);
		writel(value | (1 << 16), A1000BASE_SAFETYCRM + 0x8);
		udelay(1);
		//restore clk div
		writel(0, A1000BASE_QSPI0 + 0x8);
		writel(clkdiv, A1000BASE_QSPI0 + 0x14);
		writel(1, A1000BASE_QSPI0 + 0x8);
		//set QSPI CS external iomux
		value = readl(A1000BASE_AONCFG + 0x4);
		writel(value | (1 << 29), A1000BASE_AONCFG + 0x4);
		value = readl(A1000BASE_AONCFG + 0x204);
		writel(value & (~(1 << 29)), A1000BASE_AONCFG + 0x204);
	}
}

u32 bst_open_xip(int index, u32 size)
{
	u32 value;
	u32 clkdiv;

	/* TODO index, Open XIP qspi0 */
	clkdiv = readl(A1000BASE_QSPI0 + 0x14);
	//set QSPI CS internal ways
	value = readl(A1000BASE_AONCFG + 0x4);
	writel(value & (~(1 << 29)), A1000BASE_AONCFG + 0x4);
	//do QSPI reset
	value = readl(A1000BASE_SAFETYCRM + 0x8);
	writel(value & (~(1 << 16)), A1000BASE_SAFETYCRM + 0x8);
	udelay(1);
	writel(value | (1 << 16), A1000BASE_SAFETYCRM + 0x8);
	udelay(1);
	//set xip set 4 byte mode
	writel(0, A1000BASE_QSPI0 + 0x8);
	value = readl(A1000BASE_QSPI0 + 0x108) & (0xffffff0f);
	if (size > 0x1000000)
		writel(value | 0x80, A1000BASE_QSPI0 + 0x108);
	else
		writel(value | 0x60, A1000BASE_QSPI0 + 0x108);

	writel(1, A1000BASE_QSPI0 + 0x8);
	//open XIP
	value = readl(A1000BASE_LBLSP0 + 0x14);
	writel(value | (0x1), A1000BASE_LBLSP0 + 0x14);
	writel(value | (0x1), A1000BASE_LBLSP0 + 0x14);
	udelay(1);

	return clkdiv;
}
int board_fit_config_name_match(const char *name)
{
	/* Just empty function now - can't decide what to choose */
	debug("%s: %s\n", __func__, name);

	return 0;
}
#ifdef SPI_SPEC_BOARD_GET_FLASH
/*
 * Qspi0 get data from spi flash.
 */
int spi_flash_get_board(u32 offset, size_t len, void *buf, u32 size)
{
	u32 clkdiv;
	void *addr = (void *)(size_t)offset;

	clkdiv = bst_open_xip(0, size);

	//memcpy
	if (offset >= SZ_16M) {
		memcpy(buf, addr, len);
	} else {
		//offset < 16M and offset+len > 16M,
		//first copy >= 16M, after copy < 16M
		if ((offset + len) > SZ_16M) {
			u32 sizetmp = SZ_16M - (u32)offset;

			memcpy(buf + sizetmp, (void *)SZ_16M,
			       (offset + len) - SZ_16M);
			memcpy(buf, addr, sizetmp);
		} else {
			memcpy(buf, addr, len);
		}
	}

	bst_close_xip(0, clkdiv);

	return 0;
}
#endif

void *board_fdt_blob_setup(void)
{
	void *fdt_blob = NULL;
#ifdef CONFIG_SPL_BUILD
	/* BURN_TOOLS doesn't need that */
	fdt_blob = (void *)SPL_RELOC_FDT_START_ADDR;
#else /* CONFIG_SPL_BUILD */
	fdt_blob = (ulong *)&_end;
#endif
	return fdt_blob;
}

void board_quiesce_devices(void)
{
	//close qspi0 XIP
	bst_close_xip(0, 0);

	//close qspi1 XIP
	bst_close_xip(1, 0);
}

#ifdef CONFIG_CMD_ELF
/* For seL4 */
unsigned long do_bootelf_exec(ulong (*entry)(int, char *const[]), int argc,
			      char *const argv[])
{
	unsigned long ret;

	printf("switch to el2\n");
	ft_verify_fdt(board_fdt_blob_setup());

	board_quiesce_devices();

	ret = entry(argc, argv);
	return ret;
}
#endif

#ifdef CONFIG_CMD_GO

unsigned long do_go_exec(ulong (*entry)(int, char *const[]), int argc,
			 char *const argv[])
{
	board_quiesce_devices();

	return entry(argc, argv);
}
#endif

static int do_heart_beat(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[])
{
	struct arm_smccc_res res;

	arm_smccc_smc(BST_UBOOT_SMC_HEARTBEAT, 0, 0, 0, 0, 0, 0, 0, &res);

	return 0;
}

U_BOOT_CMD(heartbeat, 1, 0, do_heart_beat,
	   "Active heartbeat by sip smc call",
	   "");

#define IPC_BASE 0x33100000
#define REG_IPC_EN(evt_mid)		\
		((IPC_BASE)|(1 << 13)|(evt_mid << 8)|(1 << 7)|(evt_mid << 2))
#define REG_EN_ARM4 (REG_IPC_EN(4))
#define SMC_RETURN_TIMEOUT  (0x10000000)

static u32 send_smc(u32 a0_, u32 a1_, u32 a2_, u32 a3_)
{
	u32 a0 = a0_, a1 = a1_, a2 = a2_, a3 = a3_;
	u32 count = 0;
	struct arm_smccc_res res;

	while (count < SMC_RETURN_TIMEOUT) {
		arm_smccc_smc(a0, a1, a2, a3, 0, 0, 0, 0, &res);
		mdelay(1);
		count++;
		if (res.a0 == 1) {
			continue;
		} else if ((res.a0 & 0xffff0000) == 0xffff0000) {
			a0 = 0x32000003;
			a1 = res.a1;
			a2 = res.a2;
			a3 = res.a3;
			if ((res.a0 & 0x0000ffff) == 0) {
				a1 = 0;
				a2 = 0;
				a3 = 0;
			}
		} else {
			return 0;
		}
	}
	return 1;
}

static u32 get_chipId(unsigned int *chipid2, unsigned int *chipid3)
{
	unsigned int reg_base, ret = 0;
	u32 smc1[56] = {0, 0, 0, 0, 0, 0, 0, 0x6,
			0x101, 0, 0xddfa6992, 0xfb4ad599,
			0x3eeedca1, 0x4cb0619c, 0, 0,
			0x101, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0};
	u32 smc2[56] = {0x1, 0x1, 0x1, 0, 0, 0, 0, 0x4,
			0xa, 0, 0x8be41000, 0,
			0x10, 0, 0x9db5cd80, 0xffff0001,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0};
	u32 smc3[56] = {0x2, 0, 0x1, 0, 0, 0, 0, 0,
			0xa, 0, 0x8be41000, 0,
			0x10, 0, 0x9db5cd80, 0xffff0001,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0};

	reg_base = 0x8be40000;
	//init ipc arm4
	writel(0x7fffffff, REG_EN_ARM4);
	//smc 1
	memcpy(reg_base, smc1, 56*4);
	(void)send_smc(0x32000004, 0, 0x8be40000, 0);

	//smc 2
	memcpy(reg_base, smc2, 56*4);
	//magic number
	writel(0x12345678, 0x8be41000);
	ret = send_smc(0x32000004, 0, 0x8be40000, 0);
	if (ret) {
		printf("WARNING:smc timeout\r\n");
		*chipid2 = readl(0x8be41000 + 4*2);
		*chipid3 = readl(0x8be41000 + 4*3);
		//smc 3
		memcpy(reg_base, smc3, 56*4);
		(void)send_smc(0x32000004, 0, 0x8be40000, 0);

		return 1;
	} else {
		*chipid2 = readl(0x8be41000 + 4*2);
		*chipid3 = readl(0x8be41000 + 4*3);
	}
	//smc 3
	memcpy(reg_base, smc3, 56*4);
	(void)send_smc(0x32000004, 0, 0x8be40000, 0);

	return 0;
}

void get_chipFromSmc(void)
{
	const char *ch, *serial;
	char *p_serialnum = NULL;
	char *p = NULL;
	char check_ch[32];
	char *chipid = NULL;
	uint8_t value[FIT_MAX_HASH_LEN];
	unsigned int chipId2, chipId3, ret;

	ch = env_get("board_name");
	if (!ch) {
		puts("no board_name in env\r\n");
		env_set("serialnumber", "BST-fedcba9876543210");
		return;
	} else {
		memcpy(check_ch, ch, 32);
		if (strchr(check_ch, '-')) {
			p = strtok(check_ch, "-");
			p = strtok(NULL, " ");
		} else {
			p = check_ch;
		}
	}
	ret = get_chipId(&chipId2, &chipId3);
	if (ret) {
		printf("ERROR:get chipid error\r\n");
		env_set("serialnumber", "BST-fedcba9876543210");
		return;
	}
	chipid = malloc(SZ_128);
	if (!chipid) {
		puts("Not malloc chipid! Out memory!\n");
		free(chipid);
		env_set("serialnumber", "BST-fedcba9876543210");
		return;
	}
	memset((void *)chipid, 0, SZ_128);

	sprintf(chipid, "%x%x", chipId2, chipId3);
	sha256_csum_wd((unsigned char *)chipid, 16,
			       (unsigned char *)value, CHUNKSZ_SHA256);
	p_serialnum = malloc(SZ_128);
	if (!p_serialnum) {
		puts("Not malloc p_serial! Out memory!\n");
		free(chipid);
		free(p_serialnum);
		env_set("serialnumber", "BST-fedcba9876543210");
		return;
	}
	memset((void *)p_serialnum, 0, SZ_128);
	sprintf(p_serialnum, "%s-%02x%02x%02x%02x%02x%02x%02x%02x", p,
			value[0], value[1], value[2], value[3],
			value[4], value[5], value[6], value[7]);

	serial = env_get("serialnumber");
	if (!serial)
		env_set("serialnumber", p_serialnum);

	free(p_serialnum);
	free(chipid);
}

static int do_get_chipFromSmc(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[])
{
	get_chipFromSmc();

	return 0;
}
U_BOOT_CMD(getChipId, 1, 0, do_get_chipFromSmc,
	   "get chip ID and set serialnumber",
	   "");
