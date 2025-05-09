// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2020 BlackSesame Tec Ltd.
 */
#include <common.h>
#include <asm/system.h>
#include <asm/types.h>
#include <asm/io.h>
#include <asm/armv8/mmu.h>
#include <fdtdec.h>
#include <asm/sections.h>
#include <environment.h>
#include <spi.h>
#include <spi_flash.h>
#include "board.h"
#include <stdlib.h>

DECLARE_GLOBAL_DATA_PTR;

#ifdef BST_DDR_INFO_ADDR
char bst_ddr_vender[DDR_VENDER_NAME_END][DDR_NAME_LEN] = { "SAMSUNG", "MICRON",
							   "HYNIX", "OTHERS" };
char bst_ddr_frequency[DDR_FREQ_END][DDR_NAME_LEN] = {
	"3200", "3200_2D", "2667",    "2667_2D", "2133",    "1600", "1066",
	"3732", "4267",	   "3732_2D", "4267_2D", "4000_2D", "4000"
};

#endif // BST_DDR_INFO_ADDR

static struct mm_region a55_mem_map[] = {
	{ /* Flash */
	  .virt = PHYS_SYSMEM_FLASH_START,
	  .phys = PHYS_SYSMEM_FLASH_START,
	  .size = PHYS_SYSMEM_FLASH_SIZE,
	  .attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) | PTE_BLOCK_NON_SHARE |
		   PTE_BLOCK_PXN | PTE_BLOCK_UXN },
	{ /* SRAM */
	  .virt = PHYS_SYSMEM_SRAM_START,
	  .phys = PHYS_SYSMEM_SRAM_START,
	  .size = PHYS_SYSMEM_SRAM_SIZE,
	  .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_INNER_SHARE },
	{ /* Lowmem peripherals */
	  .virt = PHYS_LOWMEM_PERIPHERALS_START,
	  .phys = PHYS_LOWMEM_PERIPHERALS_START,
	  .size = PHYS_LOWMEM_PERIPHERALS_SIZE,
	  .attrs = PTE_BLOCK_MEMTYPE(MT_DEVICE_NGNRNE) | PTE_BLOCK_NON_SHARE |
		   PTE_BLOCK_PXN | PTE_BLOCK_UXN },
	{ /* SDRAM0 */
	  .virt = PHYS_SDRAM_1,
	  .phys = PHYS_SDRAM_1,
	  .size = PHYS_SDRAM_1_SIZE,
	  .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_INNER_SHARE },
	{ /* SDRAM1 */
	  .virt = PHYS_SDRAM_2,
	  .phys = PHYS_SDRAM_2,
	  .size = PHYS_SDRAM_2_SIZE,
	  .attrs = PTE_BLOCK_MEMTYPE(MT_NORMAL) | PTE_BLOCK_INNER_SHARE },
	{
		0,
	}
};

struct mm_region *mem_map = a55_mem_map;

#ifdef CONFIG_ARMV8_SPIN_TABLE
static void cpu_a55_enable(unsigned int cpu, unsigned long rvbaraddr)
{
	if (cpu & (1 << 1)) {
		CPU_CSR_RVBARADDR1_REG = rvbaraddr;
		CPU_CSR_CORE1_CLKEN_REG = 1;
		CPU_CSR_CORE1_RESET_REG = 3;
	}
	if (cpu & (1 << 2)) {
		CPU_CSR_RVBARADDR2_REG = rvbaraddr;
		CPU_CSR_CORE2_CLKEN_REG = 1;
		CPU_CSR_CORE2_RESET_REG = 3;
	}
	if (cpu & (1 << 3)) {
		CPU_CSR_RVBARADDR3_REG = rvbaraddr;
		CPU_CSR_CORE3_CLKEN_REG = 1;
		CPU_CSR_CORE3_RESET_REG = 3;
	}
	if (cpu & (1 << 4)) {
		CPU_CSR_RVBARADDR4_REG = rvbaraddr;
		CPU_CSR_CORE4_CLKEN_REG = 1;
		CPU_CSR_CORE4_RESET_REG = 3;
	}
	if (cpu & (1 << 5)) {
		CPU_CSR_RVBARADDR5_REG = rvbaraddr;
		CPU_CSR_CORE5_CLKEN_REG = 1;
		CPU_CSR_CORE5_RESET_REG = 3;
	}
	if (cpu & (1 << 6)) {
		CPU_CSR_RVBARADDR6_REG = rvbaraddr;
		CPU_CSR_CORE6_CLKEN_REG = 1;
		CPU_CSR_CORE6_RESET_REG = 3;
	}
	if (cpu & (1 << 7)) {
		CPU_CSR_RVBARADDR7_REG = rvbaraddr;
		CPU_CSR_CORE7_CLKEN_REG = 1;
		CPU_CSR_CORE7_RESET_REG = 3;
	}
}
#endif

int dram_init_banksize(void)
{
	gd->bd->bi_dram[0].start = PHYS_SDRAM_1;
	gd->bd->bi_dram[0].size = PHYS_SDRAM_1_SIZE;
#if (CONFIG_NR_DRAM_BANKS == 2)
	gd->bd->bi_dram[1].start = PHYS_SDRAM_2;
	gd->bd->bi_dram[1].size = PHYS_SDRAM_2_SIZE;
#endif

	return 0;
}

#ifdef CONFIG_BOARD_EARLY_INIT_F
int board_early_init_f(void)
{
	const int *cell;
	int node, len, value;
	u32 val;

	//	do_board_detect();

	node = fdt_path_offset(gd->fdt_blob, "/clkconfig");
	if (node < 0)
		return node;

	cell = fdt_getprop(gd->fdt_blob, node, "clk_a55cpu", &len);
	if (!cell)
		return -1;

	//	value = fdt32_to_cpu(cell[0]);
	val = (readl(A1000BASE_TOPCRM + 0x8) & (0xfff << 16)) >> 16;
	if (val == 0x38)
		gd->cpu_clk = 1400000000; /* 1.4 GHz*/
	else if (val == 0x30)
		gd->cpu_clk = 1200000000; /*1.2 GHz*/
	else
		gd->cpu_clk = 25000000; /* 25MHz */

	return 0;
}
#endif

int board_init(void)
{
	//close qspi0 XIP
	//	bst_close_xip(0, 0);

	//close qspi1 XIP
	//bst_close_xip(1, 0);

	writel(0x1234abcd, 0x18040004);
	flush_dcache_all();

	return 0;
}

#ifdef CONFIG_BOARD_EARLY_INIT_R
int board_early_init_r(void)
{
	gd->bd->bi_memstart = CONFIG_SYS_SDRAM_BASE; /* start of DRAM memory */
	gd->bd->bi_memsize =
		PHYS_SDRAM_1_SIZE; /* size	 of DRAM memory in bytes */
	gd->bd->bi_flashstart =
		PHYS_SYSMEM_FLASH_START; /* start of FLASH memory */
	gd->bd->bi_flashsize =
		PHYS_SYSMEM_FLASH_SIZE; /* size	 of FLASH memory */
	gd->bd->bi_flashoffset = SZ_2M; /* reserved area for startup monitor */
	gd->bd->bi_sramstart =
		PHYS_SYSMEM_SRAM_START; /* start of SRAM memory */
	gd->bd->bi_sramsize =
		PHYS_SYSMEM_SRAM_SIZE; /* size	 of SRAM memory */

#ifdef CONFIG_ARMV8_SPIN_TABLE
	printf("uboot release cpus .\n");
	cpu_a55_enable(0xff, gd->relocaddr / 4);
	mdelay(3);
#endif

	return 0;
}
#endif

#ifdef CONFIG_MISC_INIT_R
int misc_init_r(void)
{
	const char *ptr;
	int flag = 0;

	ptr = env_get("ddrc_interleave");
	if (ptr) {
		env_set("ddrc_interleave", NULL);
		flag = 1;
	}

	ptr = env_get("ddrc_frequency");
	if (ptr) {
		env_set("ddrc_frequency", NULL);
		flag = 1;
	}

	ptr = env_get("ddrc_ecc");
	if (ptr) {
		env_set("ddrc_ecc", NULL);
		flag = 1;
	}

	if (flag) {
		mdelay(300);
		board_init();
		env_save();
	}

	return 0;
}
#endif

int dram_init(void)
{
	gd->ram_size = PHYS_SDRAM_1_SIZE;

	return 0;
}
void *nvram_read(void *dest, const long src, size_t count)
{
	uchar *d = (uchar *)dest;
	uchar *s = (uchar *)src;

	while (count--)
		*d++ = *s++;

	return dest;
}
void nvram_write(long dest, const void *src, size_t count)
{
	int ret = 0;
	struct spi_flash *env_flash;

	env_flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
				    CONFIG_SF_DEFAULT_SPEED,
				    CONFIG_SF_DEFAULT_MODE);
	if (!env_flash) {
		puts("SPI probe failed.\n");
		goto done;
	}

	debug("Erasing SPI flash...");
	ret = spi_flash_erase(env_flash, dest, count);
	if (ret) {
		printf("Erasing SPI flash error:%d\n", ret);

		goto done;
	}

	debug("Writing to SPI flash...");

	ret = spi_flash_write(env_flash, dest, count, src);
	if (ret) {
		printf("Writing to SPI flash error:%d\n", ret);

		goto done;
	}

	debug("done\n");
done:
	return;
}

int ft_verify_fdt(void *fdt)
{
	int err;
	char s_chip_type[256];
	u32 data = 0;

	memset(s_chip_type, 0, sizeof(s_chip_type));

	sprintf(s_chip_type, "A1000A");

	err = fdt_find_and_setprop(fdt, "/", "chip_type", s_chip_type,
				   strlen(s_chip_type) + 1, 1);

	if (err < 0) {
		printf("WARNING: could not set chip_type %s.\n",
		       fdt_strerror(err));
		goto FT_VERIFY_FDT_ERR;
	}

FT_VERIFY_FDT_ERR:
	return 1;
}

#ifdef BST_DDR_INFO_ADDR

static int do_get_ddr_support_freq(struct spi_flash *flash,
				   s_ddr_config *ddr_config)
{
	//
	int ret = 0;
	int i = 0;
	u32 firmware_addr[2] = { DDR_FIRMWARE_3200_2D, DDR_FIRMWARE_3200};

	if (!flash) {
		puts("SPI flash error.\n");
		ret = -1;
		goto done;
	}
	for (i = 0; i < 2; i++) {
		ret = spi_flash_read(flash, (u32)(firmware_addr[i]),
				     sizeof(T_CFG_DDR), &ddr_config[i]);
		if (ret) {
			printf("read SPI flash error:%d\n", ret);
			goto done;
		}
	}

done:
	return ret;
}

static int do_get_ddr_info(struct spi_flash *flash, T_CFG_DDR *ddrInfo)
{
	//
	int ret = 0;

	if (!flash) {
		puts("SPI flash error.\n");
		ret = -1;
		goto done;
	}

	ret = spi_flash_read(flash, (u32)(BST_DDR_INFO_ADDR), sizeof(T_CFG_DDR),
			     ddrInfo);
	if (ret) {
		printf("read SPI flash error:%d\n", ret);
		goto done;
	}

	if (ddrInfo->ddrECC < 0 || ddrInfo->ddrFre < 0 ||
	    ddrInfo->ddrInterleave < 0 || ddrInfo->ddrVender < 0 ||
	    ddrInfo->ddrECC >= DDR_ECC_END || ddrInfo->ddrFre >= DDR_FREQ_END ||
	    ddrInfo->ddrInterleave >= DDR_INTERLEAVE_END ||
	    ddrInfo->ddrVender >= DDR_VENDER_NAME_END) {
		printf("ddr information data error.\n");
		ret = -1;
		goto done;
	}

done:
	return ret;
}

static int do_set_ddr_info(struct spi_flash *flash, T_CFG_DDR *ddrInfo)
{
	int ret = 0;

	if (!flash) {
		puts("SPI flash error.\n");
		ret = -1;
		goto done;
	}

	debug("Erasing SPI flash...");
	ret = spi_flash_erase(flash, (u32)(BST_DDR_INFO_ADDR), DDR_INFO_SIZE);
	if (ret) {
		printf("Erasing SPI flash error:%d\n", ret);

		goto done;
	}

	debug("Writing to SPI flash...");
	printf("ready to setting:\n");
	printf("ddrFre=%d  ", ddrInfo->ddrFre);
	printf("ddrECC=%d  ", ddrInfo->ddrECC);
	printf("ddrInterleave=%d\n", ddrInfo->ddrInterleave);

	ret = spi_flash_write(flash, (u32)(BST_DDR_INFO_ADDR), DDR_INFO_SIZE,
			      ddrInfo);
	if (ret) {
		printf("Writing to SPI flash error:%d\n", ret);

		goto done;
	}
done:
	return ret;
}
static int do_ddr_info(cmd_tbl_t *cmdtp, int flag, int argc, char *const argv[])
{
	const char *cmd;
	struct spi_flash *flash;
	T_CFG_DDR ddrInfo;
	int ret = 0;
	unsigned long ddrFre;
	unsigned long ddrInterleave;
	unsigned long ddrECC;
	s_ddr_config ddr_config[2];
	s_ddr_config *p_ddr_config = ddr_config;
	int i = 0;

	/* need at least 1 arguments */
	if (argc < 1)
		goto usage;

	cmd = argv[0];

	flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS, CONFIG_SF_DEFAULT_CS,
				CONFIG_SF_DEFAULT_SPEED,
				CONFIG_SF_DEFAULT_MODE);
	if (!flash) {
		puts("SPI probe failed.\n");
		goto usage;
	}
	memset(ddr_config, 0, 2 * sizeof(s_ddr_config));
	do_get_ddr_support_freq(flash, p_ddr_config);

	if (strcmp(cmd, "ddrinfoget") == 0) {
		ret = do_get_ddr_info(flash, &ddrInfo);
		if (ret != 0)
			goto done;
		printf("Vender:\t\t%s\n", bst_ddr_vender[ddrInfo.ddrVender]);
		printf("Frequency:\t%s\n", bst_ddr_frequency[ddrInfo.ddrFre]);
		printf("ECC:\t\t%s\n", ddrInfo.ddrECC ? "close" : "open");
		printf("Interleave:\t%s\n",
		       ddrInfo.ddrInterleave ? "close" : "open");
		goto done;
	} else if (strcmp(cmd, "ddrinfoset") == 0 && argc == 4) {
		ret = do_get_ddr_info(flash, &ddrInfo);
		if (ret != 0)
			goto done;

		if (strict_strtoul(argv[1], 10, &ddrFre) != 0)
			goto dataerr;
		if (strict_strtoul(argv[3], 10, &ddrInterleave) != 0)
			goto dataerr;
		if (strict_strtoul(argv[2], 10, &ddrECC) != 0)
			goto dataerr;
		if (ddrFre != ddr_config[0].ddr_fre &&
		    ddrFre != ddr_config[1].ddr_fre) {
			printf("DDR FREQ ERROR\n");
			goto dataerr;
		}
		ddrInfo.ddrFre = ddrFre;
		ddrInfo.ddrInterleave = ddrInterleave;
		ddrInfo.ddrECC = ddrECC;

		if (ddrInfo.ddrECC < 0 || ddrInfo.ddrFre < 0 ||
		    ddrInfo.ddrInterleave < 0 ||
		    ddrInfo.ddrECC >= DDR_ECC_END ||
		    ddrInfo.ddrFre >= DDR_FREQ_END ||
		    ddrInfo.ddrInterleave >= DDR_INTERLEAVE_END) {
			printf("DDR PARA OVER LIMITS\n");
			goto dataerr;
		}

		ret = do_set_ddr_info(flash, &ddrInfo);
		goto done;

	} else {
		printf("This DDR firmware only supported frequencies:");
		for (i = 0; i < 2; i++) {
			printf(" [%s:%d] ",
			       bst_ddr_frequency[ddr_config[i].ddr_fre],
			       ddr_config[i].ddr_fre);
		}
		printf("\n");
		ret = -1;
		goto usage;
	}
dataerr:
	printf("parameters error\n");
	printf("This DDR firmware only supported frequencies:");
	for (i = 0; i < 2; i++) {
		printf(" [%s:%d] ", bst_ddr_frequency[ddr_config[i].ddr_fre],
		       ddr_config[i].ddr_fre);
	}
	printf("\n");
	ret = -1;

done:
	if (ret != -1)
		return ret;

usage:
	return -1;
}
U_BOOT_CMD(ddrinfoget, 1, 0, do_ddr_info,
	   "get ddr information (vender, frequency, ecc, interleave)", "");

U_BOOT_CMD(ddrinfoset, CONFIG_SYS_MAXARGS, 0, do_ddr_info,
	   "set ddr information (frequency, ecc, interleave)",
	   "[frequency] [ecc] [interleave]\n"
	   "[frequency]\n"
	   "[ecc]\t\t0:open       1:close\n"
	   "[interleave]\t0:open       1:close\n");

#endif // BST_DDR_INFO_ADDR
