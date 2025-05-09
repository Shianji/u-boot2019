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

#include <asm/spin_table.h>

#include "board.h"

DECLARE_GLOBAL_DATA_PTR;

static void set_tbclk(unsigned long cntfrq)
{
	asm volatile("msr cntfrq_el0, %x0" : : "rZ"(cntfrq));
	isb();
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
	writel(readl(A1000BASE_SAFETYCRM + 0x8) & (~0x1),
	       A1000BASE_SAFETYCRM + 0x8);

	while (1)
		asm("nop");
}

void bst_close_xip(int index, u32 clkdiv)
{
	u32 value;

	if (clkdiv == 0) {
		if (index == 0) {
			value = readl(A1000BASE_LBLSP0 + 0x10);
			writel(value & ~BIT(0), A1000BASE_LBLSP0 + 0x10);
		} else {
			value = readl(A1000BASE_LBLSP0 + 0x10);
			writel(value & ~BIT(1), A1000BASE_LBLSP0 + 0x10);
		}
	} else {
		/* TODO index, Close XIP qspi0 */
		value = readl(A1000BASE_LBLSP0 + 0x10);
		writel(value & (~0x1), A1000BASE_LBLSP0 + 0x10);
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
	value = readl(A1000BASE_LBLSP0 + 0x10);
	writel(value | (0x1), A1000BASE_LBLSP0 + 0x10);
	writel(value | (0x1), A1000BASE_LBLSP0 + 0x10);
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
