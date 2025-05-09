// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

/* #define	DEBUG	*/

#include <common.h>
#include <autoboot.h>
#include <cli.h>
#include <console.h>
#include <version.h>

/*
 * Board-specific Platform code can reimplement show_boot_progress () if needed
 */
__weak void show_boot_progress(int val) {}

static void run_preboot_environment_command(void)
{
#ifdef CONFIG_PREBOOT
	char *p;

	p = env_get("preboot");
	if (p != NULL) {
		int prev = 0;

		if (IS_ENABLED(CONFIG_AUTOBOOT_KEYED))
			prev = disable_ctrlc(1); /* disable Ctrl-C checking */

		run_command_list(p, -1, 0);

		if (IS_ENABLED(CONFIG_AUTOBOOT_KEYED))
			disable_ctrlc(prev);	/* restore Ctrl-C checking */
	}
#endif /* CONFIG_PREBOOT */
}

/* We come here after U-Boot is initialised and ready to process commands */
void main_loop(void)
{
	const char *s;
	//添加名为"main_loop"的bootstage标签，记录时间戳等信息
	bootstage_mark_name(BOOTSTAGE_ID_MAIN_LOOP, "main_loop");

	if (IS_ENABLED(CONFIG_VERSION_VARIABLE))
		env_set("ver", version_string);  /* 设置版本环境变量 set version variable */

	cli_init();//该函数会根据配置初始化一个强大的Hush解析器（一个shell解释器）或一个简单的解释器
	(*(volatile unsigned int *)(0x32700000) = (1));
	//执行预启动环境命令，执行环境变量 preboot 中定义的命令（如网络初始化、设备检测）
	run_preboot_environment_command();
	//如果启用 TFTP 更新功能，则执行更新操作,尝试通过 TFTP 协议从网络加载固件更新（需配合 update_tftp 命令配置）
	if (IS_ENABLED(CONFIG_UPDATE_TFTP))
		update_tftp(0UL, NULL, NULL);
	//开启CONFIG_AUTOBOOT后，该函数会获取要执行的命令s
	s = bootdelay_process();
	//如果设备树（FDT）中指定了bootcmd属性，则用它覆盖当前的环境变量 bootcmd，从而改变自动启动时执行的命令。
	//如果设备树中设置了 bootsecure=1，则强制使用 FDT 中定义的 bootcmd，如果此时FDT中没有定义 bootcmd，则会导致启动失败。
	if (cli_process_fdt(&s))
		cli_secure_boot_cmd(s);
	//开启CONFIG_AUTOBOOT后，该函数会执行命令s列表，或者在倒计时到达前被打断
	autoboot_command(s);
	cli_loop();//进入u-boot命令行循环，等待用户输入命令
	panic("No CLI available");
}
