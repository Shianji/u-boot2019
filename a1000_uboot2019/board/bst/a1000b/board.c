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
#include <linux/arm-smccc.h>

DECLARE_GLOBAL_DATA_PTR;


#define kstrtoul strict_strtoul


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

	node = fdt_path_offset(gd->fdt_blob, "/clkconfig");
	if (node < 0)
		return node;

	cell = fdt_getprop(gd->fdt_blob, node, "clk_a55cpu", &len);
	if (!cell)
		return -1;

	//value = fdt32_to_cpu(cell[0]);
	value = (readl(A1000BASE_TOPCRM + 0x8) & (0xfff << 16)) >> 16;
	if (value == 0x78)
		gd->cpu_clk = 1500000000; /* 1.5 GMHz*/
	else if (value == 0x70)
		gd->cpu_clk = 1400000000; /* 1.4 GMHz*/
	else
		gd->cpu_clk = 25000000; /* 25MHz */

	return 0;
}
#endif



int board_init(void)
{
	//close qspi0 XIP
	//bst_close_xip(0, 0);

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
	gd->bd->bi_sramsize = PHYS_SYSMEM_SRAM_SIZE; /* size of SRAM memory */

#ifdef CONFIG_ARMV8_SPIN_TABLE
	printf("uboot release cpus .\n");
	cpu_a55_enable(0xff, gd->relocaddr / 4);
	mdelay(3);
#endif

	return 0;
}
#endif

/*1:open; 0:close*/
int sync_enable_ecc(unsigned int ecc_status)
{
	const char *ptr;
	int flag = 0;
	char tmp[256];

	if (0 != ecc_status && 1 != ecc_status) {
		ecc_status = 0;
		printf("ecc_status=%d\n", ecc_status);
	}

	ptr = env_get("enable_ecc");
	if (!ptr) {
		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "%d", ecc_status);
		env_set("enable_ecc", tmp);
		flag = 1;
		printf("don not have enable_ecc , set %s\n", tmp);
		goto END;
	}

	if ((ecc_status == 0 &&  *ptr != 0x30) ||
		(ecc_status == 1 &&  *ptr != 0x31)) {
		memset(tmp, 0, sizeof(tmp));
		sprintf(tmp, "%d", ecc_status);
		env_set("enable_ecc", tmp);
		flag = 1;
		printf("ecc_status=%d *p=%d\n", ecc_status, *ptr);
		goto END;
	}

END:
	if (flag) {
		mdelay(300);
		env_save();
	}

	return 0;
}

#ifdef CONFIG_MISC_INIT_R
int misc_init_r(void)
{
	const char *ptr;
	int flag = 0;
	struct spi_flash *flash;

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

	ptr = env_get("ddrc_szauto");
	if (!ptr) {
		env_set("ddrc_szauto", "1");
		flag = 1;
	}

	ptr = env_get("enable_ecc");
	if (!ptr) {
		T_CFG_DDR ddrInfo_tmp;

		flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
							CONFIG_SF_DEFAULT_CS,
							CONFIG_SF_DEFAULT_SPEED,
							CONFIG_SF_DEFAULT_MODE);
		do_get_ddr_info(flash, &ddrInfo_tmp);
		if (ddrInfo_tmp.ddrECC == 0)
			sync_enable_ecc(1);
		else
			sync_enable_ecc(0);
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



#ifdef BST_DDR_INFO_ADDR

#define BIG_TO_LITTLE_ENDIAN(value) \
		 (((value&0x000000ff)<<24) | ((value&0x0000ff00)<<8) | \
		 ((value&0x00ff0000)>>8) | ((value&0xff000000)>>24))

int do_ddr_fdt_check(struct spi_flash *flash)
{
	int ret = 0;
	struct fdt_header s_fdt_header;

	memset(&s_fdt_header, 0, sizeof(struct fdt_header));
	ret = spi_flash_read(flash, (u32)(0x200000), sizeof(struct fdt_header),
				&s_fdt_header);
	if (ret) {
		printf("read SPI flash error:%d\n", ret);
		goto done;
	}
	s_fdt_header.totalsize = BIG_TO_LITTLE_ENDIAN(s_fdt_header.totalsize);
	if (s_fdt_header.totalsize <= 0 ||
		s_fdt_header.totalsize > DTB_DDR_INFO_SIZE) {
		ret = -1;
		goto done;
	}

done:
	return ret;
}


#if 0
static int do_get_ddr_support_freq(struct spi_flash *flash,
				   s_ddr_config *ddr_config)
{
	//
	int ret = 0;
	int i = 0;
	u32 firmware_addr[2];

	firmware_addr[0] = DDR_FIRMWARE_3200_2D;
	firmware_addr[1] = DDR_FIRMWARE_3200;
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
#endif

u8 *ddr_dtb_buf;
#define FDT_DDRC_DTB_ADDR  0x200000


static int get_dtb_ddr_info(void *ddrc_dtb_addr, T_CFG_DDR *ddrInfo)
{
	int ddr_info_node, len;
	const int *cell;

	if (ddrc_dtb_addr == NULL || ddrInfo == NULL)
		return -1;
	ddr_info_node = fdt_path_offset(ddrc_dtb_addr, "/ddrc/ddr_info");
	if (ddr_info_node < 0)
		return -1;
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "vender", &len);
	if (!cell)
		return -1;
	ddrInfo->ddrVender = fdt32_to_cpu(cell[0]);
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "freq", &len);
	if (!cell)
		return -1;
	ddrInfo->ddrFre = fdt32_to_cpu(cell[0]);
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "interleave", &len);
	if (!cell)
		return -1;
	ddrInfo->ddrInterleave = fdt32_to_cpu(cell[0]);
	if (ddrInfo->ddrInterleave == 0)
		ddrInfo->ddrInterleave = 1;
	else if (ddrInfo->ddrInterleave == 1)
		ddrInfo->ddrInterleave = 0;
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "ecc", &len);
	if (!cell)
		return -1;
	ddrInfo->ddrECC = fdt32_to_cpu(cell[0]);
	if (ddrInfo->ddrECC == 0)
		ddrInfo->ddrECC = 1;
	else if (ddrInfo->ddrECC == 1)
		ddrInfo->ddrECC = 0;
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "parity", &len);
	if (!cell)
		return -1;
	ddrInfo->ddrOTHERS = fdt32_to_cpu(cell[0]);
	if (ddrInfo->ddrOTHERS == 0)
		ddrInfo->ddrOTHERS = 1;
	else if (ddrInfo->ddrOTHERS == 1)
		ddrInfo->ddrOTHERS = 0;
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "clksscg", &len);
	if (!cell)
		return -1;
	ddrInfo->clksscg = fdt32_to_cpu(cell[0]);
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "init_count", &len);
	if (!cell)
		return -1;
	ddrInfo->ddr_init_count = fdt32_to_cpu(cell[0]);
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "rank_count", &len);
	if (!cell)
		return -1;
	ddrInfo->rank_count = fdt32_to_cpu(cell[0]);
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "ecc_range", &len);
	if (!cell)
		return -1;
	ddrInfo->ecc_range = fdt32_to_cpu(cell[0]);
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "zone", &len);
	if (!cell)
		return -1;
	ddrInfo->zone = fdt32_to_cpu(cell[0]);

	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "adapt", &len);
	if (!cell)
		return -1;
	ddrInfo->adapte = fdt32_to_cpu(cell[0]);
	cell = fdt_getprop(ddrc_dtb_addr, ddr_info_node, "debug", &len);
	if (!cell)
		return -1;
	ddrInfo->debug = fdt32_to_cpu(cell[0]);
	if (ddrInfo->debug == 0)
		ddrInfo->debug = 1;
	else if (ddrInfo->debug == 1)
		ddrInfo->debug = 0;
	return 0;

}

static int set_dtb_ddr_info(void *ddrc_dtb_addr, T_CFG_DDR *ddrInfo)
{
	int ddr_info_node, ret = 0;
	u32 temp[1];

	if (ddrc_dtb_addr == NULL || ddrInfo == NULL)
		return -1;

	ddr_info_node = fdt_path_offset(ddrc_dtb_addr, "/ddrc/ddr_info");
	if (ddr_info_node < 0)
		return -1;

	temp[0] = cpu_to_fdt32(ddrInfo->ddrVender);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "vender");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "vender", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}

	temp[0] = cpu_to_fdt32(ddrInfo->ddrFre);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "freq");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "freq", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}

	if (ddrInfo->ddrInterleave == 0)
		ddrInfo->ddrInterleave = 1;
	else if (ddrInfo->ddrInterleave == 1)
		ddrInfo->ddrInterleave = 0;

	temp[0] = cpu_to_fdt32(ddrInfo->ddrInterleave);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "interleave");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "interleave", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}

	if (ddrInfo->ddrECC == 0)
		ddrInfo->ddrECC = 1;
	else if (ddrInfo->ddrECC == 1)
		ddrInfo->ddrECC = 0;

	temp[0] = cpu_to_fdt32(ddrInfo->ddrECC);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "ecc");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "ecc", temp,
				sizeof(u32));

	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}

	if (ddrInfo->ddrOTHERS == 0)
		ddrInfo->ddrOTHERS = 1;
	else if (ddrInfo->ddrOTHERS == 1)
		ddrInfo->ddrOTHERS = 0;

	temp[0] = cpu_to_fdt32(ddrInfo->ddrOTHERS);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "parity");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "parity", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}

	temp[0] = cpu_to_fdt32(ddrInfo->clksscg);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "clksscg");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "clksscg", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}

	temp[0] = cpu_to_fdt32(ddrInfo->ddr_init_count);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "init_count");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "init_count", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}

	temp[0] = cpu_to_fdt32(ddrInfo->rank_count);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "rank_count");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "rank_count", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}


	temp[0] = cpu_to_fdt32(ddrInfo->ecc_range);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "ecc_range");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "ecc_range", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}

	temp[0] = cpu_to_fdt32(ddrInfo->zone);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "zone");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "zone", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}


	temp[0] = cpu_to_fdt32(ddrInfo->adapte);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "adapt");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "adapt", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}


	if (ddrInfo->debug == 0)
		ddrInfo->debug = 1;
	else if (ddrInfo->debug == 1)
		ddrInfo->debug = 0;
	temp[0] = cpu_to_fdt32(ddrInfo->debug);
	fdt_delprop(ddrc_dtb_addr, ddr_info_node, "debug");
	ret = fdt_setprop(ddrc_dtb_addr, ddr_info_node, "debug", temp,
				sizeof(u32));
	if (ret) {
		printf("fdt set prop error\n");
		goto done;
	}

done:
	return ret;

}


static int do_get_ddr_fdt_totalsize(struct spi_flash *flash, u32 *totalsize)
{
	int ret = 0;
	struct fdt_header s_fdt_header;

	memset(&s_fdt_header, 0, sizeof(struct fdt_header));
	ret = spi_flash_read(flash, (u32)(0x200000), sizeof(struct fdt_header),
				&s_fdt_header);
	if (ret) {
		printf("read SPI flash error:%d\n", ret);
		goto done;
	}
	s_fdt_header.totalsize = BIG_TO_LITTLE_ENDIAN(s_fdt_header.totalsize);
	*totalsize = s_fdt_header.totalsize;

done:
	return ret;
}

static int do_get_ddr_info(struct spi_flash *flash, T_CFG_DDR *ddrInfo)
{
	int ret = 0;
	u32 totalsize = 0;

	if (!flash) {
		puts("SPI flash error.\n");
		ret = -1;
		goto done;
	}

	ret = do_ddr_fdt_check(flash);

	if (ret == 0) {
		printf("====== it is dtb mode ======\n");
		ret = do_get_ddr_fdt_totalsize(flash, &totalsize);
		if (ret) {
			printf("get ddr fdt totalsize fail\n");
			goto done;
		}
		if (ddr_dtb_buf == NULL) {
			ddr_dtb_buf = (u8 *)malloc(totalsize);
			if (ddr_dtb_buf == NULL) {
				printf("malloc DTB DDR buffer fail\n");
				ret = -1;
				goto done;
			}
		}

		ret = spi_flash_read(flash, (u32)(FDT_DDRC_DTB_ADDR), totalsize,
						ddr_dtb_buf);
		if (ret) {
			printf("read SPI flash error:%d\n", ret);
			goto done;
		}

		ret = get_dtb_ddr_info(ddr_dtb_buf, ddrInfo);
		if (ret) {
			printf("read DTB DDR INFO error:%d\n", ret);
			goto done;
		}

	} else {
		printf("====== it is other mode ======\n");
		ret = spi_flash_read(flash, (u32)(BST_DDR_INFO_ADDR),
					sizeof(T_CFG_DDR), ddrInfo);
		if (ret) {
			printf("read SPI flash error:%d\n", ret);
			goto done;
		}

	}

	if (ddrInfo->ddrECC >= DDR_ECC_END || ddrInfo->ddrFre >= DDR_FREQ_END ||
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
	u32 totalsize = 0;

	if (!flash) {
		puts("SPI flash error.\n");
		ret = -1;
		goto done;
	}

	ret = do_ddr_fdt_check(flash);

	if (ret == 0) {
		if (ddr_dtb_buf == NULL) {
			printf("DDR DTB buffer error\n");
			ret = -1;
			goto done;
		}
		ret = set_dtb_ddr_info(ddr_dtb_buf, ddrInfo);
		if (ret) {
			printf("set DTB DDR INFO error:%d\n", ret);
			goto done;
		}

		ret = do_get_ddr_fdt_totalsize(flash, &totalsize);
		if (ret) {
			printf("get ddr fdt totalsize fail\n");
			goto done;
		}

		debug("Erasing SPI flash...");
		ret = spi_flash_erase(flash, (u32)(FDT_DDRC_DTB_ADDR),
					DTB_DDR_INFO_SIZE);
		if (ret) {
			printf("Erasing SPI flash error:%d\n", ret);
			goto done;
		}

		debug("Writing to SPI flash...");
		printf("ready to setting:\n");
		printf("ddrFre=%d  ", ddrInfo->ddrFre);
		printf("ddrECC=%d  ", ddrInfo->ddrECC);
		printf("ddrInterleave=%d\n", ddrInfo->ddrInterleave);

		ret = spi_flash_write(flash, (u32)(FDT_DDRC_DTB_ADDR),
					DTB_DDR_INFO_SIZE, ddr_dtb_buf);
		if (ret) {
			printf("Writing to SPI flash error:%d\n", ret);
			goto done;
		}

	} else {
		debug("Erasing SPI flash...");
		ret = spi_flash_erase(flash, (u32)(BST_DDR_INFO_ADDR),
						DDR_INFO_SIZE);
		if (ret) {
			printf("Erasing SPI flash error:%d\n", ret);
			goto done;
		}
		debug("Writing to SPI flash...");
		printf("ready to setting:\n");
		printf("ddrFre=%d  ", ddrInfo->ddrFre);
		printf("ddrECC=%d  ", ddrInfo->ddrECC);
		printf("ddrInterleave=%d\n", ddrInfo->ddrInterleave);
		ret = spi_flash_write(flash, (u32)(BST_DDR_INFO_ADDR),
					DDR_INFO_SIZE, ddrInfo);
		if (ret) {
			printf("Writing to SPI flash error:%d\n", ret);
			goto done;
		}
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
	unsigned long ddrOTHERS;
	unsigned long debug;
	s_ddr_config ddr_config[3];

	// s_ddr_config *p_ddr_config = ddr_config;
	// int i = 0;

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
	ddr_config[0].ddr_fre = 0x1; //3200_2D
	ddr_config[1].ddr_fre = 0xb; //4000_2D
	ddr_config[2].ddr_fre = 0x9; //3733_2D

	if (strcmp(cmd, "ddrinfoget") == 0) {
		ret = do_get_ddr_info(flash, &ddrInfo);
		if (ret != 0)
			goto done;
		//printf("vender:\t\t%s\n", bst_ddr_vender[ddrInfo.ddrVender]);
		printf("frequency:\t%s\n", bst_ddr_frequency[ddrInfo.ddrFre]);
		printf("ecc:\t\t%s\n", ddrInfo.ddrECC ? "close" : "open");
		printf("interleave:\t%s\n",
		       ddrInfo.ddrInterleave ? "close" : "open");
		printf("parity:\t\t%s\n", ddrInfo.ddrOTHERS ? "close" : "open");
		ret = do_ddr_fdt_check(flash);
		if (ret == 0) {
			printf("debug:\t\t%s\n",
				ddrInfo.debug ? "close" : "open");
		}
		if (ddrInfo.ddrECC == 0)
			sync_enable_ecc(1);
		else
			sync_enable_ecc(0);

		goto done;
	} else if (strcmp(cmd, "ddrinfoset") == 0 && (argc == 6 || argc == 5)) {
		ret = do_get_ddr_info(flash, &ddrInfo);
		if (ret != 0)
			goto done;

		if (kstrtoul(argv[1], 10, &ddrFre) != 0)
			goto dataerr;
		if (kstrtoul(argv[3], 10, &ddrInterleave) != 0)
			goto dataerr;
		if (kstrtoul(argv[2], 10, &ddrECC) != 0)
			goto dataerr;
		if (kstrtoul(argv[4], 10, &ddrOTHERS) != 0)
			goto dataerr;
		ret = do_ddr_fdt_check(flash);
		if (ret == 0) {
			if (kstrtoul(argv[5], 10, &debug) != 0)
				goto dataerr;
		}

		if (ddrFre != ddr_config[0].ddr_fre) {
			if (ddrFre != ddr_config[1].ddr_fre) {
				if (ddrFre != ddr_config[2].ddr_fre) {
					printf("DDR FREQ ERROR ONLY 1 or 11 or 9\n");
					goto dataerr;
				}
			}
		}

		ddrInfo.ddrFre = ddrFre;
		ddrInfo.ddrInterleave = ddrInterleave;
		ddrInfo.ddrECC = ddrECC;
		ddrInfo.ddrOTHERS = ddrOTHERS;
		ret = do_ddr_fdt_check(flash);
		if (ret == 0)
			ddrInfo.debug = debug;

		if (ddrInfo.ddrECC >= DDR_ECC_END ||
		    ddrInfo.ddrFre >= DDR_FREQ_END ||
		    ddrInfo.ddrInterleave >= DDR_INTERLEAVE_END) {
			printf("DDR PARA OVER LIMITS\n");
			goto dataerr;
		}

		if (ddrECC == 0)
			sync_enable_ecc(1);
		else
			sync_enable_ecc(0);

		ret = do_set_ddr_info(flash, &ddrInfo);
		goto done;

	} else {
		printf("parameters error\n");
		printf("\n");
		ret = -1;
		goto usage;
	}

dataerr:
	printf(
	"This DDR firmware only supported frequencies:1:[3200_2D] or 9:[3733_2D] or 11:[4000_2D]");
	printf("\n");
	ret = -1;

done:
	if (ret != -1)
		return ret;

usage:
	return -1;
}
U_BOOT_CMD(ddrinfoget, 1, 0, do_ddr_info,
	   "dtb mode  : get ddr information (vender, frequency, ecc, interleave, parity, debug)\n"
	   "other mode: get ddr information (vender, frequency, ecc, interleave, parity)\n",
	   "");

U_BOOT_CMD(ddrinfoset, CONFIG_SYS_MAXARGS, 0, do_ddr_info,
	   "dtb mode  :set ddr information (frequency, ecc, interleave, parity, debug)\n"
	   "other mode:set ddr information (frequency, ecc, interleave, parity)\n",
	   "[frequency] [ecc] [interleave] [parity] [debug]\n"
	   "dtb mode  eg:ddrinfoset 1 0 0 0 0\n"
	   "other mode  eg:ddrinfoset 1 0 0 0\n"
	   "[frequency] 1 is 32002D; 9 is 37332D; 11 is 40002D\n"
	   "[ecc]\t\t0:open       1:close\n"
	   "[interleave]\t0:open       1:close\n"
	   "[parity]\t0:open       1:close\n"
	   "[debug]\t\t0:open       1:close\n");

#endif // BST_DDR_INFO_ADDR


u32 bswap_32(u32 x)
{
	return (((u32)(x) & 0xff000000) >> 24) |
	    (((u32)(x) & 0x00ff0000) >> 8) |
		(((u32)(x) & 0x0000ff00) << 8) |
		(((u32)(x) & 0x000000ff) << 24);
}

int change_to_little(u32 *value, int len)
{
	int i = 0;
	int data = 1;
	unsigned char *endian;

	endian = (unsigned char *)&data;
	if (endian[0] == 0x01) {
		for (i = 0; i < len; i++)
			value[i] = bswap_32(value[i]);
	}
	return 1;
}

static void set_firewall_by_smc(void)
{
	const char *ptr;
	u32 need_open;
	struct arm_smccc_res res;

	ptr = env_get("firewall");
	if (!ptr) {
		return;
	} else {
		need_open = simple_strtol(ptr, NULL, 16);
		if (need_open == 0) {
			return;
		} else {
			printf("open firewall for ");
			if ((need_open&0x1) == 0x1)
				printf("LSP0\t");
			if ((need_open&0x2) == 0x2)
				printf("LSP1\t");
			if ((need_open&0x4) == 0x4)
				printf("QSPI0\t");
			if ((need_open&0x8) == 0x8)
				printf("QSPI1\t");
			if ((need_open&0x10) == 0x10)
				printf("SSP\t");
			if ((need_open&0x20) == 0x20)
				printf("SRAM\t");
			printf("\r\n");

			arm_smccc_smc(BST_UBOOT_SET_FIREWALL, need_open,
							0, 0, 0, 0, 0, 0, &res);
		}
	}
}

#define GPIO0_BASE_ADDR     (0x20010000)

//#define read_reg(__addr)             (*((volatile u32 *)(__addr)))
//#define write_reg(__addr, __val)    (*((volatile u32 *)(__addr)) = (__val))

#define read_reg(__addr)                        (*((u32 *)(__addr)))
#define write_reg(__addr, __val)         (*((u32 *)(__addr)) = (__val))

int ft_verify_fdt(void *fdt)
{
	int err;
	char s_chip_type[256];
	u32 memory_change[256];
	u32 data = 0;
	T_CFG_DDR s_ddr_info;
	struct spi_flash *flash;
	int ret = 0;
	const char *ptr;
	int ddr_szauto = -1;
	void *fdt_adr;
	int is_enable_hyp = 0;
	u32 func_gpio_25;
	u32 level_gpio_25;
	u32 io_gpio_25;


	ptr = env_get("enable_hyp");
	if (!ptr) {
		printf("don not have enable_hyp\n");
		fdt_adr = images.ft_addr;
	} else {
		printf("enable hyp val %x\n", *ptr);
		if (*ptr == 0x30) {
			fdt_adr = images.ft_addr;
			is_enable_hyp = 0;
		} else if (*ptr == 0x31) {
			fdt_adr = (void *)0x80000000;
			is_enable_hyp = 1;
		} else {
			fdt_adr = images.ft_addr;
			printf("invalid enable_hyp val\n");
		}
	}

	memset(s_chip_type, 0, sizeof(s_chip_type));

	data = (readl(0x3300006c) & 0xff);
	if (data == 1)
		sprintf(s_chip_type, "A1000B0");
	else if (data == 2)
		sprintf(s_chip_type, "A1000LB0");
	else if (data == 3)
		sprintf(s_chip_type, "A1000LEB0");
	else {
		printf("WARNING: chip_type default.\n");
		sprintf(s_chip_type, "A1000B");
	}


	err = fdt_check_header(fdt_adr);
	if (err < 0) {
		set_firewall_by_smc();
		return err;
	}

	err = fdt_find_and_setprop(fdt_adr, "/", "chip_type", s_chip_type,
				   strlen(s_chip_type) + 1, 1);

	if (err < 0) {
		printf("WARNING: could not set chip_type %s.\n",
		       fdt_strerror(err));
	}

	ptr = env_get("ddrc_szauto");
	if (!ptr) {
		printf("ddrc_szauto env not found: default open!\n");
	} else {
		if (strcmp(ptr, "1") == 0) {
			//printf("ddrc interleave open\n");
			ddr_szauto = 1;
		} else if (strcmp(ptr, "0") == 0) {
			//printf("ddrc interleave close\n");
			ddr_szauto = 0;
		} else {
			printf("ddrc_szauto env Invaild: default open!\n");
			ddr_szauto = 1;
		}
	}


	if (ddr_szauto == 1) {
		memset(&s_ddr_info, 0, sizeof(s_ddr_info));
		flash = spi_flash_probe(CONFIG_SF_DEFAULT_BUS,
						CONFIG_SF_DEFAULT_CS,
						CONFIG_SF_DEFAULT_SPEED,
						CONFIG_SF_DEFAULT_MODE);
		if (!flash) {
			puts("AUTO ADAPTE 4G/8G SPI probe failed.\n");
			goto FT_VERIFY_FDT_ERR;
		}
		ret = do_get_ddr_info(flash, &s_ddr_info);
		if (ret != 0) {
			puts("AUTO ADAPTE 4G/8G do_get_ddr_info failed.\n");
			goto FT_VERIFY_FDT_ERR;
		}

	func_gpio_25 = read_reg(0xc0038000+/*32'h*/0x0);
	func_gpio_25 &= (~(0x3 << 1));
	write_reg(0xc0038000+/*32'h*/0x0, func_gpio_25);
	io_gpio_25 = read_reg(GPIO0_BASE_ADDR+/*32'h*/0x4);
	io_gpio_25 &= (~(0x1 << 25));
	write_reg(GPIO0_BASE_ADDR+/*32'h*/0x4, io_gpio_25);
	level_gpio_25 = (((read_reg(GPIO0_BASE_ADDR+/*32'h*/0x50))
		>> 25) & 0x1);
	printf("level_gpio_25=%d\n", level_gpio_25);

		if (level_gpio_25 == 0) {

			memset(memory_change, 0, 256*sizeof(u32));
			memory_change[0] = 0x0;
			memory_change[1] = 0x80000000;
			memory_change[2] = 0x0;
			memory_change[3] = 0xe0000000;
			change_to_little(memory_change, 4);
			err = fdt_find_and_setprop(fdt_adr, "/memory@80000000"
			, "reg", memory_change, 4*sizeof(u32), 1);
			if (err < 0) {
				printf("WARNING: could not set");
				printf("/memory@80000000 %s.\n"
					, fdt_strerror(err));
			}

			if (is_enable_hyp) {
				memset(memory_change, 0, 256*sizeof(u32));
				memory_change[0] = 0x1;
				memory_change[1] = 0x98000000;
				memory_change[2] = 0x0;
				memory_change[3] = 0xC8000000;
			} else {
				memset(memory_change, 0, 256*sizeof(u32));
				memory_change[0] = 0x1;
				memory_change[1] = 0x80000000;
				memory_change[2] = 0x0;
				memory_change[3] = 0xe0000000;
			}
			change_to_little(memory_change, 4);
			err = fdt_find_and_setprop(fdt_adr, "/memory@180000000"
			, "reg", memory_change, 4*sizeof(u32), 1);
			if (err < 0) {
				printf("WARNING: could not set");
				printf("/memory@180000000 %s.\n"
						, fdt_strerror(err));
			}

			memset(memory_change, 0, 256*sizeof(u32));
			memory_change[0] = 0x1;
			memory_change[1] = 0x60000000;
			memory_change[2] = 0x0;
			memory_change[3] = 0x20000000;
			change_to_little(memory_change, 4);
			err = fdt_find_and_setprop(fdt_adr,
			"/reserved-memory/ddr0@0xf0000000"
			, "reg", memory_change,
				4*sizeof(u32), 1);
			if (err < 0) {
				printf("WARNING: could not set");
				printf("/reserved-memory/bst_ddr0_reserved:");
				printf("ddr0@0xf0000000 %s.\n"
					, fdt_strerror(err));
			}

			memset(memory_change, 0, 256*sizeof(u32));
			memory_change[0] = 0x2;
			memory_change[1] = 0x60000000;
			memory_change[2] = 0x0;
			memory_change[3] = 0x20000000;
			change_to_little(memory_change, 4);
			err = fdt_find_and_setprop(fdt_adr,
			"/reserved-memory/ddr1@0x1f0000000"
			, "reg", memory_change,
				4*sizeof(u32), 1);
			if (err < 0) {
				printf("WARNING: could not set");
				printf("/reserved-memory/bst_ddr1_reserved:");
				printf("ddr1@0x1f0000000: ddr0@0xf0000000 %s.\n"
					, fdt_strerror(err));
			}
		} else if (level_gpio_25 == 1) {

			if (is_enable_hyp) {
				memset(memory_change, 0, 256*sizeof(u32));
				memory_change[0] = 0x1;
				memory_change[1] = 0x98000000;
				memory_change[2] = 0x0;
				memory_change[3] = 0x58000000;
				change_to_little(memory_change, 4);
				err = fdt_find_and_setprop(fdt_adr,
				"/memory@180000000"
				, "reg", memory_change, 4*sizeof(u32), 1);
				if (err < 0) {
					printf("WARNING: could not set");
					printf("/memory@180000000 %s.\n"
							, fdt_strerror(err));
				}
			}


		}
	}



FT_VERIFY_FDT_ERR:
	set_firewall_by_smc();
	return 1;
}

