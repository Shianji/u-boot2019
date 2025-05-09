// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2000-2009
 * Wolfgang Denk, DENX Software Engineering, wd@denx.de.
 */

/*
 *  Command Processor Table
 */

#include <common.h>
#include <command.h>
#include <console.h>
#include <linux/ctype.h>

/*
 * Use puts() instead of printf() to avoid printf buffer overflow
 * for long help messages
 */
//打印帮助信息，cmd_start为命令表的首地址，cmd_items为命令表的长度，cmdtp为命令表项的指针，flag为标志位，argc为想要打印命令的个数，argv为想要打印命令的列表，如果argc为1，则打印所有命令简短帮助信息
int _do_help(cmd_tbl_t *cmd_start, int cmd_items, cmd_tbl_t *cmdtp, int flag,
	     int argc, char * const argv[])
{
	int i;
	int rcode = 0;

	if (argc == 1) {	/* show list of commands */
		cmd_tbl_t *cmd_array[cmd_items];
		int i, j, swaps;

		/* Make array of commands from .uboot_cmd section */
		cmdtp = cmd_start;
		for (i = 0; i < cmd_items; i++) {
			cmd_array[i] = cmdtp++;
		}

		/* Sort command list (trivial bubble sort),冒泡排序，按照指针所指命令的命令名对cmd_array[]中指向命令的指针排序 */
		for (i = cmd_items - 1; i > 0; --i) {
			swaps = 0;
			for (j = 0; j < i; ++j) {
				if (strcmp(cmd_array[j]->name,
					   cmd_array[j + 1]->name) > 0) {
					cmd_tbl_t *tmp;
					tmp = cmd_array[j];
					cmd_array[j] = cmd_array[j + 1];
					cmd_array[j + 1] = tmp;
					++swaps;
				}
			}
			if (!swaps)
				break;
		}

		/* print short help (usage) */
		for (i = 0; i < cmd_items; i++) {
			const char *usage = cmd_array[i]->usage;

			/* allow user abort */
			if (ctrlc())
				return 1;
			if (usage == NULL)
				continue;
			printf("%-*s- %s\n", CONFIG_SYS_HELP_CMD_WIDTH,
			       cmd_array[i]->name, usage);
		}
		return 0;
	}
	/*
	 * command help (long version)
	 */
	for (i = 1; i < argc; ++i) {
		cmdtp = find_cmd_tbl(argv[i], cmd_start, cmd_items);
		if (cmdtp != NULL) {
			rcode |= cmd_usage(cmdtp);//打印帮助信息
		} else {
			printf("Unknown command '%s' - try 'help' without arguments for list of all known commands\n\n",
			       argv[i]);
			rcode = 1;
		}
	}
	return rcode;
}

/* find command table entry for a command *///从命令表中查找命令，cmd为待查找的命令的命令名，table为命令表的首地址，table_len为命令表的长度
cmd_tbl_t *find_cmd_tbl(const char *cmd, cmd_tbl_t *table, int table_len)
{
#ifdef CONFIG_CMDLINE
	cmd_tbl_t *cmdtp;
	cmd_tbl_t *cmdtp_temp = table;	/* Init value */
	const char *p;
	int len;
	int n_found = 0;

	if (!cmd)
		return NULL;
	/*
	 * Some commands allow length modifiers (like "cp.b");
	 * compare command name only until first dot.
	 */
	len = ((p = strchr(cmd, '.')) == NULL) ? strlen (cmd) : (p - cmd);

	for (cmdtp = table; cmdtp != table + table_len; cmdtp++) {
		if (strncmp(cmd, cmdtp->name, len) == 0) {
			if (len == strlen(cmdtp->name))
				return cmdtp;	/* full match */

			cmdtp_temp = cmdtp;	/* abbreviated command ? */
			n_found++;
		}
	}
	if (n_found == 1) {			/* exactly one match */
		return cmdtp_temp;
	}
#endif /* CONFIG_CMDLINE */

	return NULL;	/* not found or ambiguous command */
}

cmd_tbl_t *find_cmd(const char *cmd)
{
	cmd_tbl_t *start = ll_entry_start(cmd_tbl_t, cmd);
	const int len = ll_entry_count(cmd_tbl_t, cmd);
	return find_cmd_tbl(cmd, start, len);
}
//打印cmdtp对应的命令的帮助信息，包括简短帮助信息和详细帮助信息（如果CONFIG_SYS_LONGHELP被定义）
//cmdtp为命令表项的指针
int cmd_usage(const cmd_tbl_t *cmdtp)
{
	printf("%s - %s\n\n", cmdtp->name, cmdtp->usage);

#ifdef	CONFIG_SYS_LONGHELP
	printf("Usage:\n%s ", cmdtp->name);

	if (!cmdtp->help) {
		puts ("- No additional help available.\n");
		return 1;
	}

	puts(cmdtp->help);
	putc('\n');
#endif	/* CONFIG_SYS_LONGHELP */
	return 1;
}

#ifdef CONFIG_AUTO_COMPLETE
static char env_complete_buf[512];
//该函数用于命令行参数的环境变量部分的补全，当只输入命令程序名而没有输入其他参数时，会查找所有环境变量，只输入一个参数（只输入一个前缀），会查找所有前缀匹配的环境变量，其他情况均查找失败。查找成功返回找到的前缀匹配的环境变量的数量，失败返回0。
int var_complete(int argc, char * const argv[], char last_char, int maxv, char *cmdv[])
{
	int space;

	space = last_char == '\0' || isblank(last_char);

	if (space && argc == 1)//如果只输入了程序名，输入参数的是空格，就查找所有环境变量
		return env_complete("", maxv, cmdv, sizeof(env_complete_buf),
				    env_complete_buf, false);

	if (!space && argc == 2)//如果输入了程序名，而输入环境变量的只有一部分，就查找拥有该前缀的所有环境变量
		return env_complete(argv[1], maxv, cmdv,
				    sizeof(env_complete_buf),
				    env_complete_buf, false);

	return 0;
}
//该函数用于给输入的命令行的最后一个参数进行补全，最后一个参数的形式必须是${待补全参数}，否则补全失败。该函数会查找所有前缀匹配的环境变量，查找成功返回找到的前缀匹配的环境变量的数量，失败返回0。
static int dollar_complete(int argc, char * const argv[], char last_char,
			   int maxv, char *cmdv[])
{
	/* Make sure the last argument starts with a $. */
	if (argc < 1 || argv[argc - 1][0] != '$' ||
	    last_char == '\0' || isblank(last_char))
		return 0;

	return env_complete(argv[argc - 1], maxv, cmdv, sizeof(env_complete_buf),
			    env_complete_buf, true);
}

/*************************************************************************************/
//该函数用于给输入的子命令进行补全
int complete_subcmdv(cmd_tbl_t *cmdtp, int count, int argc,
		     char * const argv[], char last_char,
		     int maxv, char *cmdv[])
{
#ifdef CONFIG_CMDLINE
	const cmd_tbl_t *cmdend = cmdtp + count;
	const char *p;
	int len, clen;
	int n_found = 0;
	const char *cmd;

	/* sanity? */
	if (maxv < 2)
		return -2;

	cmdv[0] = NULL;

	if (argc == 0) {
		/* output full list of commands */
		for (; cmdtp != cmdend; cmdtp++) {
			if (n_found >= maxv - 2) {
				cmdv[n_found++] = "...";
				break;
			}
			cmdv[n_found++] = cmdtp->name;
		}
		cmdv[n_found] = NULL;
		return n_found;
	}

	/* more than one arg or one but the start of the next */
	//如果有多个参数，或只有一个参数且该参数的最后一个字符是空格或者'\0'，则查找与之匹配的命令(需要精确匹配)
	if (argc > 1 || last_char == '\0' || isblank(last_char)) {
		cmdtp = find_cmd_tbl(argv[0], cmdtp, count);
		if (cmdtp == NULL || cmdtp->complete == NULL) {
			cmdv[0] = NULL;
			return 0;
		}
		return (*cmdtp->complete)(argc, argv, last_char, maxv, cmdv);
	}

	cmd = argv[0];
	/*
	 * Some commands allow length modifiers (like "cp.b");
	 * compare command name only until first dot.
	 *///在某些命令中，可能会有类似 .b、.w、.l 这样的修饰符，这些通常用于指定操作的字节长度
	p = strchr(cmd, '.');
	if (p == NULL)
		len = strlen(cmd);
	else
		len = p - cmd;

	/* return the partial matches */
	for (; cmdtp != cmdend; cmdtp++) {

		clen = strlen(cmdtp->name);
		if (clen < len)
			continue;

		if (memcmp(cmd, cmdtp->name, len) != 0)
			continue;

		/* too many! */
		if (n_found >= maxv - 2) {
			cmdv[n_found++] = "...";
			break;
		}

		cmdv[n_found++] = cmdtp->name;
	}

	cmdv[n_found] = NULL;
	return n_found;
#else
	return 0;
#endif
}
//该函数用于给输入的命令进行补全
static int complete_cmdv(int argc, char * const argv[], char last_char,
			 int maxv, char *cmdv[])
{
#ifdef CONFIG_CMDLINE
	return complete_subcmdv(ll_entry_start(cmd_tbl_t, cmd),
				ll_entry_count(cmd_tbl_t, cmd), argc, argv,
				last_char, maxv, cmdv);
#else
	return 0;
#endif
}
//该函数用于从字符串s中提取出参数，每个参数之间用空格分隔，返回提取到的参数数量。
static int make_argv(char *s, int argvsz, char *argv[])
{
	int argc = 0;

	/* split into argv */
	while (argc < argvsz - 1) {

		/* skip any white space */
		while (isblank(*s))
			++s;

		if (*s == '\0')	/* end of s, no more args	*/
			break;

		argv[argc++] = s;	/* begin of argument string	*/

		/* find end of string */
		while (*s && !isblank(*s))
			++s;

		if (*s == '\0')		/* end of s, no more args	*/
			break;

		*s++ = '\0';		/* terminate current arg	 */
	}
	argv[argc] = NULL;

	return argc;
}
//该函数用于打印命令行参数，banner为打印的标题，leader为每行的前缀，sep为每个参数之间的分隔符，linemax为每行的最大长度，argv为待打印的参数列表
static void print_argv(const char *banner, const char *leader, const char *sep, int linemax, char * const argv[])
{
	int ll = leader != NULL ? strlen(leader) : 0;
	int sl = sep != NULL ? strlen(sep) : 0;
	int len, i;

	if (banner) {
		puts("\n");
		puts(banner);
	}

	i = linemax;	/* force leader and newline */
	while (*argv != NULL) {
		len = strlen(*argv) + sl;
		if (i + len >= linemax) {
			puts("\n");
			if (leader)
				puts(leader);
			i = ll - sl;
		} else if (sep)
			puts(sep);
		puts(*argv++);
		i += len;
	}
	printf("\n");
}
//该函数查找argv中所有参数的公共前缀，返回公共前缀的长度
static int find_common_prefix(char * const argv[])
{
	int i, len;
	char *anchor, *s, *t;

	if (*argv == NULL)
		return 0;

	/* begin with max */
	anchor = *argv++;
	len = strlen(anchor);
	while ((t = *argv++) != NULL) {
		s = anchor;
		for (i = 0; i < len; i++, t++, s++) {
			if (*t != *s)
				break;
		}
		len = s - anchor;
	}
	return len;
}

static char tmp_buf[CONFIG_SYS_CBSIZE + 1];	/* copy of console I/O buffer */
//该函数用于命令行参数的自动补全，prompt为提示符，buf为输入的命令行参数，np为输入的命令行长度值的指针，colp为当前所在列数值的指针
int cmd_auto_complete(const char *const prompt, char *buf, int *np, int *colp)
{
	int n = *np, col = *colp;
	char *argv[CONFIG_SYS_MAXARGS + 1];		/* NULL terminated	*/
	char *cmdv[20];
	char *s, *t;
	const char *sep;
	int i, j, k, len, seplen, argc;
	int cnt;
	char last_char;

	if (strcmp(prompt, CONFIG_SYS_PROMPT) != 0)
		return 0;	/* not in normal console */

	cnt = strlen(buf);
	if (cnt >= 1)
		last_char = buf[cnt - 1];
	else
		last_char = '\0';

	/* copy to secondary buffer which will be affected */
	strcpy(tmp_buf, buf);

	/* separate into argv */
	argc = make_argv(tmp_buf, sizeof(argv)/sizeof(argv[0]), argv);

	/* first try a $ completion */
	i = dollar_complete(argc, argv, last_char,
			    sizeof(cmdv) / sizeof(cmdv[0]), cmdv);
	if (!i) {
		/* do the completion and return the possible completions */
		i = complete_cmdv(argc, argv, last_char,
				  sizeof(cmdv) / sizeof(cmdv[0]), cmdv);
	}

	/* no match; bell and out */
	if (i == 0) {
		if (argc > 1)	/* allow tab for non command */
			return 0;
		putc('\a');
		return 1;
	}

	s = NULL;
	len = 0;
	sep = NULL;
	seplen = 0;
	if (i == 1) { /* one match; perfect *///仅有一个匹配项
		if (last_char != '\0' && !isblank(last_char))
			k = strlen(argv[argc - 1]);
		else
			k = 0;

		s = cmdv[0] + k;
		len = strlen(s);
		sep = " ";
		seplen = 1;
	} else if (i > 1 && (j = find_common_prefix(cmdv)) != 0) { /* more */
		if (last_char != '\0' && !isblank(last_char))
			k = strlen(argv[argc - 1]);
		else
			k = 0;

		j -= k;//计算剩余待补全的长度j
		if (j > 0) {
			s = cmdv[0] + k;
			len = j;
		}
	}

	if (s != NULL) {//s不为空才需要在当前输入的命令行上进行补全
		k = len + seplen;
		/* make sure it fits */
		if (n + k >= CONFIG_SYS_CBSIZE - 2) {
			putc('\a');
			return 1;
		}

		t = buf + cnt;//在buf中将可补全的部分补全
		for (i = 0; i < len; i++)
			*t++ = *s++;
		if (sep != NULL)
			for (i = 0; i < seplen; i++)
				*t++ = sep[i];
		*t = '\0';
		n += k;
		col += k;
		puts(t - k);
		if (sep == NULL)
			putc('\a');
		*np = n;
		*colp = col;
	} else {
		print_argv(NULL, "  ", " ", 78, cmdv);

		puts(prompt);
		puts(buf);
	}
	return 1;
}

#endif

#ifdef CMD_DATA_SIZE
int cmd_get_data_size(char* arg, int default_size)
{
	/* Check for a size specification .b, .w or .l.
	 */
	int len = strlen(arg);
	if (len > 2 && arg[len-2] == '.') {
		switch (arg[len-1]) {
		case 'b':
			return 1;
		case 'w':
			return 2;
		case 'l':
			return 4;
#ifdef CONFIG_SYS_SUPPORT_64BIT_DATA
		case 'q':
			return 8;
#endif
		case 's':
			return -2;
		default:
			return -1;
		}
	}
	return default_size;
}
#endif

#if defined(CONFIG_NEEDS_MANUAL_RELOC)
DECLARE_GLOBAL_DATA_PTR;
//该函数用于实现cmd中的指针在内存中的重定位，主要是将cmd的指针加上gd->reloc_off（gd->reloc_off应该是一个全局变量，是系统重定位偏移量），cmdtp指向需要重定位的命令的首地址，size是需要重定位的命令的数量
void fixup_cmdtable(cmd_tbl_t *cmdtp, int size)
{
	int	i;

	if (gd->reloc_off == 0)//gd->reloc_off 存储了重定位偏移量。如果该值为零，说明系统没有发生重定位，因此直接返回，不需要做任何处理。
		return;

	for (i = 0; i < size; i++) {
		ulong addr;

		addr = (ulong)(cmdtp->cmd) + gd->reloc_off;
#ifdef DEBUG_COMMANDS
		printf("Command \"%s\": 0x%08lx => 0x%08lx\n",
		       cmdtp->name, (ulong)(cmdtp->cmd), addr);
#endif
		cmdtp->cmd =
			(int (*)(struct cmd_tbl_s *, int, int, char * const []))addr;
		addr = (ulong)(cmdtp->name) + gd->reloc_off;
		cmdtp->name = (char *)addr;
		if (cmdtp->usage) {
			addr = (ulong)(cmdtp->usage) + gd->reloc_off;
			cmdtp->usage = (char *)addr;
		}
#ifdef	CONFIG_SYS_LONGHELP
		if (cmdtp->help) {
			addr = (ulong)(cmdtp->help) + gd->reloc_off;
			cmdtp->help = (char *)addr;
		}
#endif
#ifdef CONFIG_AUTO_COMPLETE
		if (cmdtp->complete) {
			addr = (ulong)(cmdtp->complete) + gd->reloc_off;
			cmdtp->complete =
				(int (*)(int, char * const [], char, int, char * []))addr;
		}
#endif
		cmdtp++;
	}
}
#endif

int cmd_always_repeatable(cmd_tbl_t *cmdtp, int flag, int argc,
			  char * const argv[], int *repeatable)
{
	*repeatable = 1;

	return cmdtp->cmd(cmdtp, flag, argc, argv);
}

int cmd_never_repeatable(cmd_tbl_t *cmdtp, int flag, int argc,
			 char * const argv[], int *repeatable)
{
	*repeatable = 0;

	return cmdtp->cmd(cmdtp, flag, argc, argv);
}

int cmd_discard_repeatable(cmd_tbl_t *cmdtp, int flag, int argc,
			   char * const argv[])
{
	int repeatable;

	return cmdtp->cmd_rep(cmdtp, flag, argc, argv, &repeatable);
}

/**
 * Call a command function. This should be the only route in U-Boot to call
 * a command, so that we can track whether we are waiting for input or
 * executing a command.
 *
 * @param cmdtp		Pointer to the command to execute
 * @param flag		Some flags normally 0 (see CMD_FLAG_.. above)
 * @param argc		Number of arguments (arg 0 must be the command text)
 * @param argv		Arguments
 * @param repeatable	Can the command be repeated
 * @return 0 if command succeeded, else non-zero (CMD_RET_...)
 */
static int cmd_call(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[],
		    int *repeatable)
{
	int result;

	result = cmdtp->cmd_rep(cmdtp, flag, argc, argv, repeatable);
	if (result)
		debug("Command failed, result=%d\n", result);
	return result;
}

enum command_ret_t cmd_process(int flag, int argc, char * const argv[],
			       int *repeatable, ulong *ticks)
{
	enum command_ret_t rc = CMD_RET_SUCCESS;
	cmd_tbl_t *cmdtp;

	/* Look up command in command table */
	cmdtp = find_cmd(argv[0]);
	if (cmdtp == NULL) {
		printf("Unknown command '%s' - try 'help'\n", argv[0]);
		return 1;
	}

	/* found - check max args */
	if (argc > cmdtp->maxargs)
		rc = CMD_RET_USAGE;

#if defined(CONFIG_CMD_BOOTD)
	/* avoid "bootd" recursion */
	else if (cmdtp->cmd == do_bootd) {
		if (flag & CMD_FLAG_BOOTD) {
			puts("'bootd' recursion detected\n");
			rc = CMD_RET_FAILURE;
		} else {
			flag |= CMD_FLAG_BOOTD;
		}
	}
#endif

	/* If OK so far, then do the command */
	if (!rc) {
		int newrep;

		if (ticks)
			*ticks = get_timer(0);
		rc = cmd_call(cmdtp, flag, argc, argv, &newrep);
		if (ticks)
			*ticks = get_timer(*ticks);
		*repeatable &= newrep;
	}
	if (rc == CMD_RET_USAGE)
		rc = cmd_usage(cmdtp);
	return rc;
}

int cmd_process_error(cmd_tbl_t *cmdtp, int err)
{
	if (err == CMD_RET_USAGE)
		return CMD_RET_USAGE;

	if (err) {
		printf("Command '%s' failed: Error %d\n", cmdtp->name, err);
		return CMD_RET_FAILURE;
	}

	return CMD_RET_SUCCESS;
}
