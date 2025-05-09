// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2003
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

/*
 * Misc boot support
 */
#include <common.h>
#include <command.h>
#include <net.h>
#include <boot_info.h>
#include <asm/arch-a1000b/a1000-addr.h>
#ifdef CONFIG_CMD_GO

/* Allow ports to override the default behavior */
__attribute__((weak))
#if 0
unsigned long do_go_exec(ulong (*entry)(int, char * const []), int argc,
				 char * const argv[])
{
	return entry(argc, argv);
}
#else
extern void (*__callout_cls_start []) (void) __attribute__((weak));
extern void (*__callout_cls_end []) (void) __attribute__((weak));
extern void (*_callout_start []) (void) __attribute__((weak));
extern void (*_callout_end []) (void) __attribute__((weak));

typedef struct boot_arg_t
{
	unsigned long  len;
	unsigned long  *code;
	unsigned long  *code_end;
	unsigned long  *cls;
	unsigned long  *cls_end;
	unsigned long  *info;
	unsigned long  *info_end;
}boot_arg;
boot_arg g_boot_arg;
boot_info g_boot_info;
unsigned long do_go_exec2(ulong (*entry)(boot_arg *), int argc,
				  char *const argv[])
 {
	bootinfo_init(&g_boot_info);
 	g_boot_arg.len = (char *)__callout_cls_end - (char *)_callout_start;
  	g_boot_arg.code = (unsigned long  *)_callout_start;
 	g_boot_arg.code_end = (unsigned long  *)_callout_end;
 	g_boot_arg.cls = (unsigned long  *)__callout_cls_start;
  	g_boot_arg.cls_end = (unsigned long  *)__callout_cls_end;
	g_boot_arg.info = (unsigned long  *)&g_boot_info;
	char *tmp = (char *)&g_boot_info;
	g_boot_arg.info_end =  (unsigned long  *)(tmp + sizeof(boot_info));
	g_boot_arg.len += sizeof(boot_info);

	 return entry (&g_boot_arg);
 }
#endif
static int do_go(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	ulong	addr, rc;
	int     rcode = 0;

	if (argc < 2)
		return CMD_RET_USAGE;

	addr = simple_strtoul(argv[1], NULL, 16);

	printf("#boot# Starting application at 0x%08lX ...\n", addr);

	cleanup_before_linux(); // flush cache before entering qnx

	/*
	 * pass address parameter as argv[0] (aka command name),
	 * and all remaining args
	 */
	rc = do_go_exec2((void *)addr, argc - 1, argv + 1);
	if (rc != 0)
		rcode = 1;

	printf("## Application terminated, rc = 0x%lX\n", rc);
	return rcode;
}

/* -------------------------------------------------------------------- */

U_BOOT_CMD(
	go, CONFIG_SYS_MAXARGS, 1,	do_go,
	"start application at address 'addr'",
	"addr [arg ...]\n    - start application at address 'addr'\n"
	"      passing 'arg' as arguments"
);

#endif

U_BOOT_CMD(
	reset, 1, 0,	do_reset,
	"Perform RESET of the CPU",
	""
);

#ifdef CONFIG_CMD_POWEROFF
U_BOOT_CMD(
	poweroff, 1, 0,	do_poweroff,
	"Perform POWEROFF of the device",
	""
);
#endif
