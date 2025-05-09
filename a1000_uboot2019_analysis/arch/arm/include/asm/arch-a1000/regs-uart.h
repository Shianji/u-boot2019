/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2011 Marek Vasut <marek.vasut@gmail.com>
 */
/*
 *(C) Copyright Black Sesame Technologies (Shanghai)Ltd. Co., 2020. All rights reserved.
 *
 * This file contains proprietary information that is the sole intellectual property of Black
 * Sesame Technologies (Shanghai)Ltd. Co. No part of this material or its documentation 
 * may be reproduced, distributed, transmitted, displayed or published in any manner 
 * without the written permission of Black Sesame Technologies (Shanghai)Ltd. Co.
 */

#ifndef __BST_UART_H_
#define __BST_UART_H_

struct bst_uart_regs {
	union {
		uint32_t	rbr; /* 0x0 */
		uint32_t	dll;
		uint32_t	thr;
	};
	union {
		uint32_t	dlh; /* 0x4 */
		uint32_t	ier;
	};
	union {
		uint32_t	fcr; /* 0x8 */
		uint32_t	iir;
	};
	uint32_t	lcr; /* 0xc */
	uint32_t	mcr; /* 0x10 */
	uint32_t	lsr; /* 0x14 */
	uint32_t	msr; /* 0x18 */
	uint32_t	scr; /* 0x1c */
	uint32_t	lpdll; /* 0x20 */
	uint8_t	reserved0[0x7c - 0x24];
	uint32_t	usr; /* 0x7c */
	uint8_t	reserved1[0xc0 - 0x80];
#define BST_UART_DLF_LEN 6
	uint32_t	dlf; /* 0xc0 */
};

#define	IER_DMAE	BIT(7)
#define	IER_UUE		BIT(6)
#define	IER_NRZE	BIT(5)
#define	IER_RTIOE	BIT(4)
#define	IER_MIE		BIT(3)
#define	IER_RLSE	BIT(2)
#define	IER_TIE		BIT(1)
#define	IER_RAVIE	BIT(0)

#define	IIR_FIFOES1	BIT(7)
#define	IIR_FIFOES0	BIT(6)
#define	IIR_TOD		BIT(3)
#define	IIR_IID2	BIT(2)
#define	IIR_IID1	BIT(1)
#define	IIR_IP		BIT(0)

#define	FCR_ITL2	BIT(7)
#define	FCR_ITL1	BIT(6)
#define	FCR_RESETTF	BIT(2)
#define	FCR_RESETRF	BIT(1)
#define	FCR_TRFIFOE	BIT(0)
#define	FCR_ITL_1	0
#define	FCR_ITL_8	(FCR_ITL1)
#define	FCR_ITL_16	(FCR_ITL2)
#define	FCR_ITL_32	(FCR_ITL2|FCR_ITL1)

#define	LCR_DLAB	BIT(7)
#define	LCR_SB		BIT(6)
#define	LCR_STKYP	BIT(5)
#define	LCR_EPS		BIT(4)
#define	LCR_PEN		BIT(3)
#define	LCR_STB		BIT(2)
#define	LCR_WLS1	BIT(1)
#define	LCR_WLS0	BIT(0)

#define	LSR_FIFOE	BIT(7)
#define	LSR_TEMT	BIT(6)
#define	LSR_TDRQ	BIT(5)
#define	LSR_BI		BIT(4)
#define	LSR_FE		BIT(3)
#define	LSR_PE		BIT(2)
#define	LSR_OE		BIT(1)
#define	LSR_DR		BIT(0)

#define	MCR_LOOP	BIT(4)
#define	MCR_OUT2	BIT(3)
#define	MCR_OUT1	BIT(2)
#define	MCR_RTS		BIT(1)
#define	MCR_DTR		BIT(0)

#define	MSR_DCD		BIT(7)
#define	MSR_RI		BIT(6)
#define	MSR_DSR		BIT(5)
#define	MSR_CTS		BIT(4)
#define	MSR_DDCD	BIT(3)
#define	MSR_TERI	BIT(2)
#define	MSR_DDSR	BIT(1)
#define	MSR_DCTS	BIT(0)

#define USR_RFF		BIT(4)
#define USR_RFNE	BIT(3)
#define USR_TFE		BIT(2)
#define USR_TFNF	BIT(1)
#define USR_BUSY	BIT(0)

#endif /*_UART_H*/
