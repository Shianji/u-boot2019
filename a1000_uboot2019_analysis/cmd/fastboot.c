// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2008 - 2009 Windriver, <www.windriver.com>
 * Author: Tom Rix <Tom.Rix@windriver.com>
 *
 * (C) Copyright 2014 Linaro, Ltd.
 * Rob Herring <robh@kernel.org>
 */
#include <common.h>
#include <command.h>
#include <console.h>
#include <g_dnl.h>
#include <fastboot.h>
#include <net.h>
#include <usb.h>
#include <watchdog.h>

static int do_fastboot_udp(int argc, char *const argv[],
			   uintptr_t buf_addr, size_t buf_size)
{
#if CONFIG_IS_ENABLED(UDP_FUNCTION_FASTBOOT)//仅在配置了 CONFIG_UDP_FUNCTION_FASTBOOT 时编译有效代码，否则直接返回错误
	int err = net_loop(FASTBOOT);//net_loop 是一个网络循环函数，用于处理网络事件，例如接收和发送数据。FASTBOOT 是一个宏，表示当前的协议模式为 fastboot。

	if (err < 0) {
		printf("fastboot udp error: %d\n", err);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
#else
	pr_err("Fastboot UDP not enabled\n");
	return CMD_RET_FAILURE;
#endif
}

static int do_fastboot_usb(int argc, char *const argv[],
			   uintptr_t buf_addr, size_t buf_size)
{
#if CONFIG_IS_ENABLED(USB_FUNCTION_FASTBOOT)//使用 CONFIG_IS_ENABLED(USB_FUNCTION_FASTBOOT) 检查是否启用了 USB Fastboot 功能。如果没有启用，直接返回错误。
	int controller_index;
	char *usb_controller;
	char *endp;
	int ret;

	if (argc < 2)
		return CMD_RET_USAGE;

	usb_controller = argv[1];
	controller_index = simple_strtoul(usb_controller, &endp, 0);//解析并验证USB控制器编号（通常为 0 或 1）
	if ((*endp != '\0') || ((controller_index != 0) &&
		(controller_index != 1))) {
		pr_err("Error: Wrong USB controller index format\n");
		return CMD_RET_FAILURE;
	}
	ret = usb_gadget_initialize(controller_index);//初始化USB硬件控制器
	if (ret) {
		pr_err("USB init failed: %d\n", ret);
		return CMD_RET_FAILURE;
	}

	g_dnl_clear_detach();
	ret = g_dnl_register("usb_dnl_fastboot");//向USB Gadget层注册Fastboot功能
	if (ret)
		goto exit;

	if (!g_dnl_board_usb_cable_connected()) {//检查USB是否物理连接，防止无意义等待
		puts("\rUSB cable not detected.\n");
		puts("Command exit.\n");
		ret = CMD_RET_FAILURE;
		goto exit;
	}

	while (1) {//持续处理USB通信，直到主机断开或用户中断
		if (g_dnl_detach())//条件1：主机断开
			break;
		if (ctrlc()) {//条件2：用户强制退出
			puts("ctrlc exit fastboot\n");
			break;
		}
		WATCHDOG_RESET();
		usb_gadget_handle_interrupts(controller_index);//处理主机的Fastboot命令
	}

	ret = CMD_RET_SUCCESS;

exit:
	g_dnl_unregister();
	g_dnl_clear_detach();
	usb_gadget_release(controller_index);

	return ret;
#else
	pr_err("Fastboot USB not enabled\n");
	return CMD_RET_FAILURE;
#endif
}

static int get_serialnumber_from_env(void)
{
	const char *ch;
	char *p_serialnum = NULL;

	p_serialnum = malloc(SZ_128);
	if (!p_serialnum) {
		puts("Not malloc p_serial! Out memory!\n");
		return 0;
	}

	memset((void *)p_serialnum, 0, SZ_128);
	ch = env_get("serialnumber");
	if (!ch)
		sprintf(p_serialnum, "%s", "BST");
	else
		sprintf(p_serialnum, "%s", ch);
	g_dnl_set_serialnumber(p_serialnum);
	free(p_serialnum);

	return 0;
}

static int do_fastboot(cmd_tbl_t *cmdtp, int flag, int argc,
		       char *const argv[])
{
	uintptr_t buf_addr = (uintptr_t) NULL;
	size_t buf_size = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	while (argc > 1 && **(argv + 1) == '-') {
		char *arg = *++argv;

		--argc;
		while (*++arg) {
			switch (*arg) {
			case 'l':
				if (--argc <= 0)
					return CMD_RET_USAGE;
				buf_addr = simple_strtoul(*++argv, NULL, 16);
				goto NXTARG;

			case 's':
				if (--argc <= 0)
					return CMD_RET_USAGE;
				buf_size = simple_strtoul(*++argv, NULL, 16);
				goto NXTARG;

			default:
				return CMD_RET_USAGE;
			}
		}
NXTARG:
		;
	}

	/* Handle case when USB controller param is just '-' */
	if (argc == 1) {
		pr_err("Error: Incorrect USB controller index\n");
		return CMD_RET_USAGE;
	}

	fastboot_init((void *)buf_addr, buf_size);

	if (!strcmp(argv[1], "udp"))
		return do_fastboot_udp(argc, argv, buf_addr, buf_size);

	if (!strcmp(argv[1], "usb")) {
		argv++;
		argc--;
	}
	get_serialnumber_from_env();

	return do_fastboot_usb(argc, argv, buf_addr, buf_size);
}

#ifdef CONFIG_SYS_LONGHELP
static char fastboot_help_text[] =
	"[-l addr] [-s size] usb <controller> | udp\n"
	"\taddr - address of buffer used during data transfers ("
__stringify(CONFIG_FASTBOOT_BUF_ADDR) ")\n"
"\tsize - size of buffer used during data transfers ("
__stringify(CONFIG_FASTBOOT_BUF_SIZE) ")";
#endif

U_BOOT_CMD(fastboot, CONFIG_SYS_MAXARGS, 1, do_fastboot,
	   "run as a fastboot usb or udp device", fastboot_help_text);
