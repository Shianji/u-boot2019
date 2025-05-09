/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) 2020 Well Chen <qiwei.chen@bst.ai>
 */

#ifndef __SERIAL_BST_H
#define __SERIAL_BST_H

/*
 * struct bst_serial_platdata - information about a BST port
 *
 * @base:               Uart port base register address
 * baudrtatre:          Uart port baudrate
 */
struct bst_serial_platdata {
	struct bst_uart_regs *base;
	int baudrate;
};

#endif /* __SERIAL_BST_H */
