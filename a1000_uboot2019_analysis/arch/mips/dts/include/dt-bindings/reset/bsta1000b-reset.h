/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (C) 2020 Bst <yi.zhang@bst.ai>
 *
 * Derived from linux/include/dt-bindings/reset/bst-a1000b-resets.h
 */
#ifndef __DT_BINDINGS_RESET_BSTA1000B_H
#define __DT_BINDINGS_RESET_BSTA1000B_H

/*TOP_CRM_REG_R_TOP_CRM_BLOCK_SW_RST0 0x3300217c*/
#define RST_CODEC_SW            3
#define RST_CPU_SW              2
#define RST_LPDDR0_SW           1
#define RST_LPDDR1_SW           0

/*TOP_CRM_REG_R_TOP_CRM_BLOCK_SW_RST1 0x33002180*/
#define RST_USB3_SW           16
#define RST_USB2_SW           15
#define RST_ISP_SW            14
#define RST_NET_SW            13
#define RST_CV_SW             12
#define RST_VSP_SW            11
#define RST_PCIE_SW           10
#define RST_GPU_SW            9
#define RST_GDMA_SW           8
#define RST_MIPI0_SW          7
#define RST_MIPI1_SW          6
#define RST_MIPI2_SW          5
#define RST_MIPI3_SW          4
#define RST_GMAC0_SW          3
#define RST_GMAC1_SW          2
#define RST_SDEMMC0_SW        1
#define RST_SDEMMC1_SW        0

/* SEC_SAFE_SYS_CTRL_R_SEC_SAFE_RESET_CTRL 0x70035008 */
#define RST_QSPI1_SW         15
#define RST_QSPI0_SW         16

/* LB_LSP0_TOP_R_LSP0_LOCAL_SF_RST_CTRL_REG 0x20020000*/
#define RST_LSP0_UART1_SW    12
#define RST_I2C0_SW           0
#define RST_I2C1_SW           1
#define RST_I2C2_SW           2
#define RST_I2SM_SW           3
#define RST_LSP0_GPIO_SW      4
#define RST_LSP0_UART0_SW     5 
#define RST_SPI0_SW           6
#define RST_WDT0_SW           7
#define RST_WDT1_SW           8
#define RST_CAN_FD0_SW        9
#define RST_LSP0_TIMER1_SW   10
#define RST_LSP0_TIMER0_SW   11


#endif /* __DT_BINDINGS_RESET_BSTA1000B_H */
