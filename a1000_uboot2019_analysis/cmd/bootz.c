// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

#include <common.h>
#include <bootm.h>
#include <command.h>
#include <lmb.h>
#include <linux/compiler.h>

int __weak bootz_setup(ulong image, ulong *start, ulong *end)
{
	/* Please define bootz_setup() for your platform */

	puts("Your platform's zImage format isn't supported yet!\n");
	return -1;
}

/*
 * zImage booting support
 */
static int bootz_start(cmd_tbl_t *cmdtp, int flag, int argc,
			char * const argv[], bootm_headers_t *images)
{
	int ret;
	ulong zi_start, zi_end;

	ret = do_bootm_states(cmdtp, flag, argc, argv, BOOTM_STATE_START,
			      images, 1);

	/* 设置内核默认加载地址，如果传入的参数中没有设置则使用默认地址Setup Linux kernel zImage entry point */
	if (!argc) {
		images->ep = load_addr;
		debug("*  kernel: default image load address = 0x%08lx\n",
				load_addr);
	} else {
		images->ep = simple_strtoul(argv[0], NULL, 16);
		debug("*  kernel: cmdline image address = 0x%08lx\n",
			images->ep);
	}

	//检查 zImage 的格式，并计算它的实际内存范围，在哪定义的呢
	ret = bootz_setup(images->ep, &zi_start, &zi_end);
	if (ret != 0)
		return 1;
	//在 LMB（Logical Memory Block，U-Boot 的内存管理器）中保留 zImage 占用的内存区域。
	lmb_reserve(&images->lmb, images->ep, zi_end - zi_start);

	/*
	 * Handle the BOOTM_STATE_FINDOTHER state ourselves as we do not
	 * have a header that provide this informaiton.
	 */
	// 查找并验证启动所需的附加镜像（如 设备树二进制文件（DTB）、初始 RAM 磁盘（initrd/ramdisk） 和 可选的第二阶段引导程序），并更新 images 结构体
	if (bootm_find_images(flag, argc, argv))
		return 1;

	return 0;
}

int do_bootz(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	int ret;

	/* Consume 'bootz' */
	argc--; argv++;
	//初始化启动镜像信息结构体images
	if (bootz_start(cmdtp, flag, argc, argv, &images))
		return 1;

	/*
	 * We are doing the BOOTM_STATE_LOADOS state ourselves, so must
	 * disable interrupts ourselves
	 */
	bootm_disable_interrupts();

	images.os.os = IH_OS_LINUX;
	ret = do_bootm_states(cmdtp, flag, argc, argv,
#ifdef CONFIG_SYS_BOOT_RAMDISK_HIGH
			      BOOTM_STATE_RAMDISK |
#endif
			      BOOTM_STATE_OS_PREP | BOOTM_STATE_OS_FAKE_GO |
			      BOOTM_STATE_OS_GO,
			      &images, 1);

	return ret;
}

#ifdef CONFIG_SYS_LONGHELP
static char bootz_help_text[] =
	"[addr [initrd[:size]] [fdt]]\n"
	"    - boot Linux zImage stored in memory\n"
	"\tThe argument 'initrd' is optional and specifies the address\n"
	"\tof the initrd in memory. The optional argument ':size' allows\n"
	"\tspecifying the size of RAW initrd.\n"
#if defined(CONFIG_OF_LIBFDT)
	"\tWhen booting a Linux kernel which requires a flat device-tree\n"
	"\ta third argument is required which is the address of the\n"
	"\tdevice-tree blob. To boot that kernel without an initrd image,\n"
	"\tuse a '-' for the second argument. If you do not pass a third\n"
	"\ta bd_info struct will be passed instead\n"
#endif
	"";
#endif

U_BOOT_CMD(
	bootz,	CONFIG_SYS_MAXARGS,	1,	do_bootz,
	"boot Linux zImage image from memory", bootz_help_text
);
