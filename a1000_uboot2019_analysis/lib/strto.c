/*
 *  linux/lib/vsprintf.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 */

/* vsprintf.c -- Lars Wirzenius & Linus Torvalds. */
/*
 * Wirzenius wrote this portably, Torvalds fucked it up :-)
 */

#include <common.h>
#include <errno.h>
#include <linux/ctype.h>

/* from lib/kstrtox.c */
//该函数根据传入的进制（传入的base处的值）或者自动推断进制（base处为0时），去掉s的前缀（只是对于16进制去掉了前面的0x，其他进制没改变前缀），返回修改后的指针
static const char *_parse_integer_fixup_radix(const char *s, unsigned int *base)
{
	if (*base == 0) {
		if (s[0] == '0') {
			if (tolower(s[1]) == 'x' && isxdigit(s[2]))
				*base = 16;
			else
				*base = 8;
		} else
			*base = 10;
	}
	if (*base == 16 && s[0] == '0' && tolower(s[1]) == 'x')
		s += 2;
	return s;
}
//该函数将cp传入的base进制（可为0、8、10、16）的字符串转换为unsigned long返回，endp经处理后指向传入的字符串cp的结尾
unsigned long simple_strtoul(const char *cp, char **endp,
				unsigned int base)
{
	unsigned long result = 0;
	unsigned long value;

	cp = _parse_integer_fixup_radix(cp, &base);

	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp-'0' : (islower(*cp)
	    ? toupper(*cp) : *cp)-'A'+10) < base) {
		result = result*base + value;
		cp++;
	}

	if (endp)
		*endp = (char *)cp;

	return result;
}

int strict_strtoul(const char *cp, unsigned int base, unsigned long *res)
{
	char *tail;
	unsigned long val;
	size_t len;

	*res = 0;
	len = strlen(cp);
	if (len == 0)
		return -EINVAL;

	val = simple_strtoul(cp, &tail, base);
	if (tail == cp)
		return -EINVAL;

	if ((*tail == '\0') ||
		((len == (size_t)(tail - cp) + 1) && (*tail == '\n'))) {
		*res = val;
		return 0;
	}

	return -EINVAL;
}

long simple_strtol(const char *cp, char **endp, unsigned int base)
{
	if (*cp == '-')
		return -simple_strtoul(cp + 1, endp, base);

	return simple_strtoul(cp, endp, base);
}

unsigned long ustrtoul(const char *cp, char **endp, unsigned int base)
{
	unsigned long result = simple_strtoul(cp, endp, base);
	switch (tolower(**endp)) {
	case 'g':
		result *= 1024;
		/* fall through */
	case 'm':
		result *= 1024;
		/* fall through */
	case 'k':
		result *= 1024;
		(*endp)++;
		if (**endp == 'i')
			(*endp)++;
		if (**endp == 'B')
			(*endp)++;
	}
	return result;
}

unsigned long long ustrtoull(const char *cp, char **endp, unsigned int base)
{
	unsigned long long result = simple_strtoull(cp, endp, base);
	switch (tolower(**endp)) {
	case 'g':
		result *= 1024;
		/* fall through */
	case 'm':
		result *= 1024;
		/* fall through */
	case 'k':
		result *= 1024;
		(*endp)++;
		if (**endp == 'i')
			(*endp)++;
		if (**endp == 'B')
			(*endp)++;
	}
	return result;
}

unsigned long long simple_strtoull(const char *cp, char **endp,
					unsigned int base)
{
	unsigned long long result = 0, value;

	cp = _parse_integer_fixup_radix(cp, &base);

	while (isxdigit(*cp) && (value = isdigit(*cp) ? *cp - '0'
		: (islower(*cp) ? toupper(*cp) : *cp) - 'A' + 10) < base) {
		result = result * base + value;
		cp++;
	}

	if (endp)
		*endp = (char *) cp;

	return result;
}

long trailing_strtoln(const char *str, const char *end)
{
	const char *p;

	if (!end)
		end = str + strlen(str);
	if (isdigit(end[-1])) {
		for (p = end - 1; p > str; p--) {
			if (!isdigit(*p))
				return simple_strtoul(p + 1, NULL, 10);
		}
	}

	return -1;
}

long trailing_strtol(const char *str)
{
	return trailing_strtoln(str, NULL);
}
