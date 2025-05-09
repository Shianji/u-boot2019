// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2020 Well Chen <qiwei.chen@bst.ai>
 *
 */

#include <common.h>
#include <asm/arch/regs-uart.h>
#include <asm/io.h>
#include <dm.h>
#include <dm/platform_data/serial_bst.h>
#include <linux/compiler.h>
#include <serial.h>
#include <watchdog.h>

DECLARE_GLOBAL_DATA_PTR;

static uint32_t bst_serial_get_baud_divider(int baudrate)
{
	return (UART_SRC_CLK << (BST_UART_DLF_LEN - 4)) / baudrate;
}

static void bst_serial_hw_init_clk_rst(uint32_t uart_index, int enable)
{
#if 0
	uint32_t clk_reg, clk_offset, reg;

	clk_reg = UART_CLK_REG;
	clk_offset = UART_CLK_BASE << port;

	reg = readl(clk_reg);

	if (enable)
		reg |= clk_offset;
	else
		reg &= ~clk_offset;

	writel(reg, clk_reg);
#endif
}

/*
 * Enable clock and set baud rate, parity etc.
 */
static void bst_serial_setbrg_common(struct bst_uart_regs *uart_regs,
				int port, int baudrate)
{
	uint32_t divider = bst_serial_get_baud_divider(baudrate);

	if (!divider)
		hang();

	bst_serial_hw_init_clk_rst(port, 1);

	/* Disable interrupts and Enable FIFOs */
	writel(0, &uart_regs->ier);
	writel(1, &uart_regs->fcr);

	/* Disable flow ctrl */
	writel(0, &uart_regs->mcr);

	/* Clear rts */
	writel((readl(&uart_regs->mcr) | MCR_RTS), &uart_regs->mcr);

	/* Enable access DLL & DLH */
	writel((readl(&uart_regs->lcr) | LCR_DLAB), &uart_regs->lcr);

	/* Set baud rate */
#ifdef CONFIG_ZEBU
	writel(1, &uart_regs->dll);
	writel(0, &uart_regs->dlh);
	writel(0, &uart_regs->dlf);
#else
	writel((divider >> BST_UART_DLF_LEN) & 0xff, &uart_regs->dll);
	writel((divider >> (BST_UART_DLF_LEN + 8)) & 0xff, &uart_regs->dlh);
	writel(divider & (BIT(BST_UART_DLF_LEN) - 1), &uart_regs->dlf);
#endif

	/* Clear DLAB bit */
	writel((readl(&uart_regs->lcr) & (~LCR_DLAB)), &uart_regs->lcr);

	/* Set data length to 8 bit, 1 stop bit, no parity */
	writel(readl(&uart_regs->lcr) | (LCR_WLS1 | LCR_WLS0), &uart_regs->lcr);
}

#if defined CONFIG_SPL_SERIAL_SUPPORT && defined CONFIG_SPL_BUILD

static void bst_serial_setbrg(void)
{
	struct bst_uart_regs *uart_regs =
			(struct bst_uart_regs *)BST_UART0_BASE;

	if (!gd->baudrate)
		gd->baudrate = CONFIG_BAUDRATE;

	mdelay(50);
	bst_serial_setbrg_common(uart_regs, 0, gd->baudrate);
}

static int bst_serial_init(void)
{
	bst_serial_setbrg();

	return 0;
}

static void bst_serial_putc(const char ch)
{
	struct bst_uart_regs *uart_regs =
			(struct bst_uart_regs *)BST_UART0_BASE;

	if (ch == '\n')
		bst_serial_putc('\r');

	/* Wait for last character to go. */
	while (!(readl(&uart_regs->lsr) & LSR_TEMT))
		WATCHDOG_RESET();

	writel(ch, &uart_regs->thr);
}

static int bst_serial_tstc(void)
{
	struct bst_uart_regs *uart_regs =
			(struct bst_uart_regs *)BST_UART0_BASE;

	return readl(&uart_regs->lsr) & LSR_DR ? 1 : 0;
}

static int bst_serial_getc(void)
{
	struct bst_uart_regs *uart_regs =
			(struct bst_uart_regs *)BST_UART0_BASE;

	/* Wait for a character to arrive. */
	while (!(readl(&uart_regs->lsr) & LSR_DR))
		WATCHDOG_RESET();

	return readl(&uart_regs->rbr) & 0xff;
}

static void bst_serial_puts(const char *s)
{
	while (*s)
		bst_serial_putc(*s++);
}

struct serial_device bst_serial_device0 = {
	.name = "spl serial 0",
	.start = bst_serial_init,
	.stop = NULL,
	.setbrg = bst_serial_setbrg,
	.getc = bst_serial_getc,
	.tstc = bst_serial_tstc,
	.putc = bst_serial_putc,
	.puts = bst_serial_puts,
};

__weak struct serial_device *default_serial_console(void)
{
	return &bst_serial_device0;
}

#else

static int bst_serial_putc(struct udevice *dev, const char ch)
{
	struct bst_serial_platdata *plat = dev->platdata;
	struct bst_uart_regs *uart_regs = (struct bst_uart_regs *)plat->base;

	/* Wait for last character to go. */
	if (!(readl(&uart_regs->lsr) & LSR_TEMT))
		return -EAGAIN;

	writel(ch, &uart_regs->thr);

	return 0;
}

static int bst_serial_getc(struct udevice *dev)
{
	struct bst_serial_platdata *plat = dev->platdata;
	struct bst_uart_regs *uart_regs = (struct bst_uart_regs *)plat->base;

	/* Wait for a character to arrive. */
	if (!(readl(&uart_regs->lsr) & LSR_DR))
		return -EAGAIN;

	return readl(&uart_regs->rbr) & 0xff;
}

static int bst_serial_setbrg(struct udevice *dev, int baudrate)
{
	struct bst_serial_platdata *plat = dev->platdata;
	struct bst_uart_regs *uart_regs = (struct bst_uart_regs *)plat->base;

	if (!gd->baudrate)
		gd->baudrate = plat->baudrate;

	bst_serial_setbrg_common(uart_regs, 0, gd->baudrate);

	return 0;
}

static int bst_serial_pending(struct udevice *dev, bool input)
{
	struct bst_serial_platdata *plat = dev->platdata;
	struct bst_uart_regs *uart_regs = (struct bst_uart_regs *)plat->base;

	if (input)
		return readl(&uart_regs->lsr) & LSR_DR ? 1 : 0;
	else
		return readl(&uart_regs->lsr) & LSR_TEMT ? 0 : 1;

	return 0;
}

static int bst_serial_probe(struct udevice *dev)
{
	struct bst_serial_platdata *plat = dev->platdata;

	bst_serial_setbrg(dev, plat->baudrate);

	return 0;
}

static int bst_serial_ofdata_to_platdata(struct udevice *dev)
{
	struct bst_serial_platdata *plat = dev->platdata;
	fdt_addr_t addr;
	int baudrate;

	addr = devfdt_get_addr(dev);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->base = (struct bst_uart_regs *)addr;

	baudrate = fdtdec_get_int(gd->fdt_blob, dev_of_offset(dev),
				"baudrate", CONFIG_BAUDRATE);
	if (baudrate == FDT_ADDR_T_NONE)
		return -EINVAL;

	plat->baudrate = baudrate;

	return 0;
}

static const struct dm_serial_ops bst_serial_ops = {
	.putc		= bst_serial_putc,
	.pending	= bst_serial_pending,
	.getc		= bst_serial_getc,
	.setbrg		= bst_serial_setbrg,
};

static const struct udevice_id bst_serial_ids[] = {
	{ .compatible = "bst,a1000-dw-uart" },
	{ }
};

U_BOOT_DRIVER(serial_bst) = {
	.name	= "serial_bst",
	.id	= UCLASS_SERIAL,
	.of_match = bst_serial_ids,
	.ofdata_to_platdata = bst_serial_ofdata_to_platdata,
	.platdata_auto_alloc_size = sizeof(struct bst_serial_platdata),
	.probe	= bst_serial_probe,
	.ops	= &bst_serial_ops,
	.flags	= DM_FLAG_PRE_RELOC,
};

#endif
