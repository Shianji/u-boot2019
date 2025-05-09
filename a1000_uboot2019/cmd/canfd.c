// SPDX-License-Identifier: GPL-2.0+
/*
 * (C) Copyright 2021
 */

/* canfd self test */

//#define DEBUG

#include <common.h>
#include <command.h>
#include <dm.h>
#include <errno.h>
#include <asm/io.h>

#define CANFD0_BASE_ADDR (0x20016000)
#define CANFD1_BASE_ADDR (0x20017000)

#define CAN0_BASE_ADDR	CANFD0_BASE_ADDR
#define CAN1_BASE_ADDR	CANFD1_BASE_ADDR

#define CAN_GLBCTRL_REG 0x0000
#define CAN_TMCTRL0_REG 0x0004
#define CAN_TMCTRL1_REG 0x0008
#define CAN_ID_REG 0x000c
#define CAN_ID_MASK_REG 0x0010
#define CAN_SEND_ID_REG 0x0014

#define CAN_TX_DATA0_REG 0x0018
#define CAN_TX_DATA1_REG 0x001c
#define CAN_TX_DATA2_REG 0x0020
#define CAN_TX_DATA3_REG 0x0024
#define CAN_TX_DATA4_REG 0x0028
#define CAN_TX_DATA5_REG 0x002c
#define CAN_TX_DATA6_REG 0x0030
#define CAN_TX_DATA7_REG 0x0034
#define CAN_TX_DATA8_REG 0x0038
#define CAN_TX_DATA9_REG 0x003c
#define CAN_TX_DATA10_REG 0x0040
#define CAN_TX_DATA11_REG 0x0044
#define CAN_TX_DATA12_REG 0x0048
#define CAN_TX_DATA13_REG 0x004c
#define CAN_TX_DATA14_REG 0x0050
#define CAN_TX_DATA15_REG 0x0054

#define CAN_RX_FIFO_DATA_REG  0x0058

#define CAN_RX_DATA0_REG 0x0058
#define CAN_RX_DATA1_REG 0x005c
#define CAN_RX_DATA2_REG 0x0060
#define CAN_RX_DATA3_REG 0x0064
#define CAN_RX_DATA4_REG 0x0068
#define CAN_RX_DATA5_REG 0x006c
#define CAN_RX_DATA6_REG 0x0070
#define CAN_RX_DATA7_REG 0x0074
#define CAN_RX_DATA8_REG 0x0078
#define CAN_RX_DATA9_REG 0x007c
#define CAN_RX_DATA10_REG 0x0080
#define CAN_RX_DATA11_REG 0x0084
#define CAN_RX_DATA12_REG 0x0088
#define CAN_RX_DATA13_REG 0x008c
#define CAN_RX_DATA14_REG 0x0090
#define CAN_RX_DATA15_REG 0x0094

#define CAN_TXERR_CNT_REG 0x0098
#define CAN_RXERR_CNT_REG 0x009c
#define CAN_REC_CTRLBIT_REG 0x00a0
#define CAN_REC_ID_REG 0x00a4
#define CAN_OVERWRITE_JUDGE_REG 0x00a8
#define CAN_IRQ_TYPE_REG 0x00ac
#define CAN_ERR_TYPE_REG 0x00b0
#define CAN_REC_TYPE_REG 0x00b4
#define CAN_STATUS_MASK_REG 0x00b8
#define CAN_ARB_LOST_CAPTURE_REG 0x00bc
#define CAN_STATUS_REG 0x00c0
#define CAN_PARITY_RESIDUAL_CTRL_REG 0x00c4

/* can gblctrl register bit */
#define iso_en  21
#define filter_mode 17
#define switch_rate  10
#define reset_signal 0

#define AON_PINMUX   0x70038008

#define set_bit(data, bit, val) ((data&(~(1<<bit)))|(val<<bit))
#define get_bit(data, bit) ((data>>bit)&0x1)
#define clear_bit(data, bit) (data & (~(1<<bit)))
#define set_bits(data, bit, mask, val) ((data&(~(mask<<bit)))|(val<<bit))

typedef enum can_dlc_type_e {
	CAN_DLC_0 = 0x0,
	CAN_DLC_1 = 0x1,
	CAN_DLC_2 = 0x2,
	CAN_DLC_3 = 0x3,
	CAN_DLC_4 = 0x4,
	CAN_DLC_5 = 0x5,
	CAN_DLC_6 = 0x6,
	CAN_DLC_7 = 0x7,
	CAN_DLC_8 = 0x8,
	CAN_DLC_9 = 0x9,
	CAN_DLC_10 = 0xa,
	CAN_DLC_11 = 0xb,
	CAN_DLC_12 = 0xc,
	CAN_DLC_13 = 0xd,
	CAN_DLC_14 = 0xe,
	CAN_DLC_15 = 0xf
} can_dlc_type;

typedef enum can_controller_type_e {
	CAN_BASIC_FRAME = 0x0,
	CANFD_BASIC_FRAME = 0x1
} can_ctrller_type;

static u32 can_dlc_2_len[16] = {
	0,
	1,
	2,
	3,
	4,
	5,
	6,
	7,
	8,
	12,
	16,
	20,
	24,
	32,
	48,
	64
};


static u32 can_test_data_pattern0[16] = {
	0x01020304, 0x05060708, 0x090a0b0c, 0x0d0e0f10,
	0x11121314, 0x15161718, 0x191a1b1c, 0x1d1e1f20,
	0x21222324, 0x25262728, 0x292a2b2c, 0x2d2e2f30,
	0x31323334, 0x35363738, 0x393a3b3c, 0x3d3e3f40
};
static u32 can_test_data_pattern1[16] = {
	 0x01030507, 0x090b0d0f, 0x11131517, 0x191b1d1f,
	 0x21232527, 0x292b2d2f, 0x31333537, 0x393b3d3f,
	 0x41434547, 0x494b4d4f, 0x51535557, 0x595b5d5f,
	 0x61636567, 0x696b6d6f, 0x71737577, 0x797b7d7f
};
static u32 can_test_data_pattern2[16] = {
	 0x02040608, 0x0a0c0e10, 0x12141618, 0x1a1c1e20,
	 0x22242628, 0x2a2c2e30, 0x32343638, 0x3a3c3e40,
	 0x42444648, 0x4a4c4e50, 0x52545658, 0x5a5c5e60,
	 0x62646668, 0x6a6c6e70, 0x72747678, 0x7a7c7e80
};

/* last_mode_flag [Last mode]: 0 loopback; 1 A tx B */
static int canfd0_last_mode_flag;
static int canfd1_last_mode_flag;


/*-----------------------------------------------------------------------
 * Definitions
 */

void can_timing_cfg(unsigned long base)
{
	unsigned long tim0_reg;
	unsigned long tim1_reg;
	unsigned int val;

	tim0_reg = CAN_TMCTRL0_REG + base; /* time ctl0 */
	tim1_reg = CAN_TMCTRL1_REG + base; /* time ctl1 */

	val = readl(tim0_reg);
	val = set_bits(val, 22, 0x3, 0x2); /* sjw=2 data */
	val = set_bits(val, 19, 0x7, 0x3); /* seg2=3 */
	val = set_bits(val, 15, 0xf, 0x5); /* seg1=5 */
	val = set_bits(val, 11, 0x3, 0x3); /* sjw=3 normal */
	val = set_bits(val, 6, 0xf, 0x8); /* seg2=8 */
	val = set_bits(val, 0, 0xf, 0xa); /* seg1=10 */
	writel(val, tim0_reg);
	debug("timing0 cfg\r\n");

	/* time_quanta_n */
	val = readl(tim1_reg);
	val = set_bits(val, 10, 0x3ff, 0xa);
	val = set_bits(val, 0, 0x3ff, 0xa); /* normal m(n)=10 */
	writel(val, tim1_reg);
	debug("timing1 cfg\r\n");
}



void can_pinmux_init(void)
{
	/* pinmux bit18~bit21 0 */
	unsigned int val = readl(AON_PINMUX);

	val = val & (~(0xf << 18));
	writel(val, AON_PINMUX);
}

static inline void can_reset_cfg(unsigned long can_base_addr)
{
	unsigned int val = 0;
	unsigned long cfg_addr = can_base_addr + CAN_GLBCTRL_REG;

	/* be sure not loopback mode */
	val = readl(cfg_addr);
	val = clear_bit(20, cfg_addr);
	writel(val, cfg_addr);

	val = readl(cfg_addr);
	val = set_bit(val, reset_signal, 0);
	writel(val, cfg_addr);
}

/* return :   0--bus ready;  -1--bus busy */
static inline int can_wait_bus_ready(unsigned long can_base_addr)
{
	unsigned int val = 0;

	volatile unsigned int i = 1;
	unsigned long status_addr = can_base_addr + CAN_STATUS_REG;

	/* read canfd bus on reday */
	val = readl(status_addr);
	while (((val>>2)&0x1) != 0x1) {
		udelay(50);
		val = readl(status_addr);
		if (i > 0x1000)
			return -1;

		i++;
	}

	return 0;
}

static inline void can_filter_enable(unsigned long can_base_addr)
{
	unsigned int val = 0;
	unsigned long cfg_addr = can_base_addr + CAN_GLBCTRL_REG;

	/* can filter mode enable */
	val = readl(cfg_addr);
	val = set_bit(val, filter_mode, 1);
	writel(val, cfg_addr);
}

static inline void can_tx_request(unsigned long can_base_addr)
{
	unsigned int val = 0;
	unsigned long cfg_addr = can_base_addr + CAN_GLBCTRL_REG;

	/* tx request */
	val = readl(cfg_addr);
	val = set_bit(val, 1, 1);
	writel(val, cfg_addr);
}

/* can basic data frame */
/*
 *  3’d0: can basic remote frame,
 *  3’d1: can basic data frame,
 *  3’d2: canfd basic data frame,
 *  3’d3: can extent remote frame,
 *  3’d4: can extent data frame,
 *  3’d5: canfd extent data frame.
 */
static inline void can_frame_dlc_cfg(unsigned long can_base_addr,
					can_dlc_type dlc_type,
					can_ctrller_type ctrller_tpye)
{
	unsigned int val = 0;
	unsigned long cfg_addr = can_base_addr + CAN_GLBCTRL_REG;
	unsigned int frame_type = 0;
	unsigned int switch_bus_flag = 0;

	if (ctrller_tpye == CAN_BASIC_FRAME) {
		frame_type = 1;
		switch_bus_flag = 0;

	} else if (ctrller_tpye == CANFD_BASIC_FRAME) {
		frame_type = 2;
		switch_bus_flag = 1;
	}

	/* can basic data frame */
	val = readl(cfg_addr);
	val = set_bits(val, 2, 7, frame_type);
	writel(val, cfg_addr);
	debug("canfd0 can basic data frame = %d\r\n", frame_type);

	/* canfd DLC */
	val = readl(cfg_addr);
	val = set_bits(val, 6, 0xf, dlc_type);
	writel(val, cfg_addr);
	debug("canfd0 canfd DLC %u\r\n", dlc_type);

	/* switch bus rate */
	val = readl(cfg_addr);
	val = set_bit(val, switch_rate, switch_bus_flag);
	writel(val, cfg_addr);
}

static inline void can_id_cfg(unsigned long tx_base_addr,
					unsigned long rx_base_addr,
					unsigned int send_id,
					unsigned int filter_id)
{
	unsigned long tx_send_id_addr = tx_base_addr + CAN_SEND_ID_REG;
	unsigned long tx_id_addr = tx_base_addr + CAN_ID_REG;
	unsigned long rx_id_addr = rx_base_addr + CAN_ID_REG;

	/* config send id */
	/* canfd basic data frame,bit[28:18]is send id，0x4 */
	writel(send_id, tx_send_id_addr);
	debug("canfd0 config send id 0x%x\r\n", send_id>>18);

	/* config filter id */
	/* Reject receive messages by yourself send */
	writel(filter_id, tx_id_addr);
	debug("canfd0 config recv id 0x345678\r\n");

	/* config filter id, mask default 0 */
	/* bit29 0 representative data frame，canfd_A rx match canfd_B tx */
	writel(0x4, rx_id_addr);
	debug("canfd1 config recv id 0x4\r\n");
}

static inline void can_send_data(unsigned long can_base_addr,
					const unsigned int *p_sdata,
					can_dlc_type dlc_type)
{
	unsigned int i = 0;
	int len = 0;
	int cnt = 0;
	unsigned long tx_data_base_addr = can_base_addr + CAN_TX_DATA0_REG;

	/* must be confrom protocol */
	len = can_dlc_2_len[dlc_type];
	cnt = (len / 4 + ((len%4) ? 1:0));
	debug("dlc_type = %d, len = %d, cnt = %d\n", dlc_type, len, cnt);

	printf("tx data:\n");
	for (i = 0; i < cnt; i++) {
		writel(p_sdata[i], tx_data_base_addr + sizeof(int)*i);
		printf("[0x%8x] ", p_sdata[i]);
	}
	printf("\n");

	/* enable controller send */
	can_tx_request(can_base_addr);
}


/* wait tx and rx success signal */
/* return :   0--bus ready;  1--bus busy */
static inline int wait_tx_to_rx(unsigned long tx_base_addr,
					unsigned long rx_base_addr)
{
	unsigned int val = 0;
	volatile unsigned int i = 0;
	unsigned long tx_data_base_addr = tx_base_addr + CAN_IRQ_TYPE_REG;
	unsigned long rx_data_base_addr = rx_base_addr + CAN_IRQ_TYPE_REG;

	/* tx irq type */
	val = readl(tx_data_base_addr);
	while (((val>>1)&0x1) != 0x1) {
		udelay(50);
		val = readl(tx_data_base_addr);

		if (i > 0x1000)
			return -2;

		i++;
	}

	/* rx irq type */
	val = readl(rx_data_base_addr);
	i = 0;
	while (((val>>2)&0x1) != 0x1) {
		udelay(50);
		val = readl(rx_data_base_addr);
		if (i > 0x1000)
			return -1;

		i++;
	}

	return 0;
}

static inline void clear_irq_status(unsigned long tx_base_addr,
					unsigned long rx_base_addr)
{
	/* could tx failed , status 0x14 */
	readl(tx_base_addr + CAN_IRQ_TYPE_REG);
	readl(rx_base_addr + CAN_IRQ_TYPE_REG);
}


/* 0 success; -1 wait timeout, rx failed */
static int can_rx_fifo_no_empty(unsigned long can_base_addr)
{
	volatile unsigned int i = 0;
	unsigned int val = 0;
	unsigned long status_addr = can_base_addr + CAN_STATUS_REG;

	val = readl(status_addr);

	while (!(val & (1<<4))) {
		udelay(50);
		val = readl(status_addr);
		if (i > 0x1000)
			return -1;

		i++;
	}

	return 0;
}

static inline void can_rec_data(unsigned long can_base_addr,
					unsigned int *p_rdata)
{
	unsigned int val = 0;
	unsigned int i = 0;
	int len = 0;
	int cnt = 0;
	unsigned long rx_data_base_addr = can_base_addr + CAN_RX_DATA0_REG;

	/* read rx frame type */
	val = readl(CAN_REC_TYPE_REG + can_base_addr);
	debug("except 2 or 20 recv_frame_type is 0x%x\r\n", val);

	/* read rx DLC */
	val = readl(CAN_REC_CTRLBIT_REG + can_base_addr);
	val = (val>>25) & 0xF;
	debug("DLC is %u\r\n", val);

	/* read rxfifo */
	if (val == 0) {
		debug("canfd1 DLC(%u) no data\r\n", val);
	} else {
		debug("canfd1 rx data is:\r\n");
		len = can_dlc_2_len[val];
		cnt = (len / 4 + ((len%4)?1:0));

		debug("dlc_type = %d, len = %d, cnt = %d\n", val, len, cnt);

		printf("rx data:\n");
		for (i = 0; i < cnt; i++) {
			p_rdata[i] = readl(rx_data_base_addr + sizeof(int)*i);
			printf("[0x%8x] ", p_rdata[i]);
		}
		printf("\n");
	}
}


int can_tx_to_xtor_can_basic(unsigned long tx_base_addr,
					unsigned long rx_base_addr,
					can_dlc_type dlc_type,
					unsigned int *p_sdata,
					can_ctrller_type ctrller_type)
{
	unsigned int can_read_buf[16] = {0,};
	int len = 0;
	int ret = -1;

	can_pinmux_init();

	can_timing_cfg(tx_base_addr);
	can_timing_cfg(rx_base_addr);

	can_reset_cfg(tx_base_addr);
	can_reset_cfg(rx_base_addr);
	clear_irq_status(tx_base_addr, rx_base_addr);

	if (can_wait_bus_ready(tx_base_addr)) {
		printf("canfd_tx bus busy\n");
		return -1;
	}

	if (can_wait_bus_ready(rx_base_addr)) {
		printf("canfd_rx bus busy\n");
		return -1;
	}

	can_filter_enable(tx_base_addr);
	can_filter_enable(rx_base_addr);

	can_frame_dlc_cfg(tx_base_addr, dlc_type, ctrller_type);
	can_id_cfg(tx_base_addr, rx_base_addr, 0x123456, 0x345678);
	can_send_data(tx_base_addr, p_sdata, dlc_type);

	/* wait tx and rx success signal */
	ret = wait_tx_to_rx(tx_base_addr, rx_base_addr);
	if (-2 == ret) {
		printf("canfd_tx failed\n");
		return -1;
	} else if (-1 == ret) {
		printf("canfd_rx failed\n");
		return -1;
	}

	can_rec_data(rx_base_addr, can_read_buf);

	len = can_dlc_2_len[dlc_type];
	if (len == 8) {
		if (memcmp(can_read_buf, p_sdata, len))
			return -1;
	}

	return 0;
}

int can_loopback_can_basic(unsigned long tx_base_addr,
					unsigned long rx_base_addr,
					can_dlc_type dlc_type,
					unsigned int *p_sdata,
					can_ctrller_type ctrller_type)
{
	unsigned int can_read_buf[16] = {0,};
	int len, val;
	int ret = -1;
	int i = 0;
	unsigned int last_mode_flag = 0;

	can_timing_cfg(tx_base_addr);
	can_reset_cfg(tx_base_addr);

	can_filter_enable(tx_base_addr);
	can_frame_dlc_cfg(tx_base_addr, dlc_type, ctrller_type);

	/* can loopback */
	val = readl(tx_base_addr);
	val = set_bit(val, 20, 1);
	writel(val, tx_base_addr);


	can_id_cfg(tx_base_addr, rx_base_addr, 0x123456, 0x345678);
	can_send_data(tx_base_addr, p_sdata, dlc_type);

#if 1
	ret = can_rx_fifo_no_empty(rx_base_addr);
	if (ret) {
		printf("canfd_tx failed\n");
		return -1;
	}
#endif

#if 1

	/* When the external channel is disconnected, */
	/* switch to loopback mode and need to read fifo */
	if (rx_base_addr == CAN0_BASE_ADDR) {
		if (canfd0_last_mode_flag)
			last_mode_flag = 1;
	} else if (rx_base_addr == CAN1_BASE_ADDR) {
		if (canfd1_last_mode_flag)
			last_mode_flag = 1;
	}

	debug("last_mode_flag = %d\n", last_mode_flag);

	if (last_mode_flag) {
		for (i = 0; i < 16; i++)
			readl(rx_base_addr + CAN_RX_DATA0_REG + sizeof(int)*i);
	}

	clear_irq_status(tx_base_addr, rx_base_addr);
#endif

	can_rec_data(rx_base_addr, can_read_buf);

	len = can_dlc_2_len[dlc_type];
	if (len == 8) {
		if (memcmp(can_read_buf, p_sdata, len))
			return -1;
	}

	return 0;
}


/*
 * iso enable control bit
 * 0: non-iso
 * 1: iso 11898-1:2015
 */
unsigned int can_gblctrl_iso(unsigned long can_base_addr, int enable)
{
	unsigned long gblctl = 0x0;
	unsigned int val = 0x0;

	gblctl = can_base_addr + CAN_GLBCTRL_REG;

	val = readl(gblctl);
	if (enable) {
		val = set_bit(val, iso_en, 1);
		debug("iso 11898-1:2015\r\n");
	} else {
		val = set_bit(val, iso_en, 0);
		debug("non-iso\r\n");
	}
	writel(val, gblctl);

	return 0;
}

/* cmd_name tx_mode tx_dir
 * cmd_name：canfd
 * tx_mode：0 canfd_0 backloop；1 canfd_1 backloop
 * tx_dir：0 canfd_0 tx to canfd_1；1 canfd_1 tx to canfd_0；
 */
int cmd_canfd(cmd_tbl_t *cmdtp, int flag, int argc, char * const argv[])
{
	can_ctrller_type ctrller_type = 0;
	int dlc_type = 8;
	int data_pattern = 0;
	int tx_dir = 0;
	int ret = -1;
	int tx_mode = 0;
	unsigned int *can_test_data = NULL;

	if ((argc != 3) && (argc != 6))
		return CMD_RET_USAGE;

	tx_mode = simple_strtoul(argv[1], NULL, 16);
	tx_dir = simple_strtoul(argv[2], NULL, 16);
	ctrller_type = CANFD_BASIC_FRAME;
	dlc_type = CAN_DLC_8;
	data_pattern = 0;

	if (argc == 6) {
		printf("can send data format : MSB\n");
		ctrller_type = simple_strtoul(argv[3], NULL, 16);
		dlc_type = simple_strtoul(argv[4], NULL, 16);
		data_pattern = simple_strtoul(argv[5], NULL, 16);
	}

	can_gblctrl_iso(CAN0_BASE_ADDR, 1);
	can_gblctrl_iso(CAN1_BASE_ADDR, 1);

	/* cfg pattern mode */
	if (data_pattern == 1)
		can_test_data = can_test_data_pattern1;
	else if (data_pattern == 2)
		can_test_data = can_test_data_pattern2;
	else
		can_test_data = can_test_data_pattern0;

	/* loopback mode */
	if (tx_mode == 0) {
		/* controller_0 to controller_0 */
		if (tx_dir == 0) {
			/* loopback canfd_0 to canfd_0 */
			printf("loopback controller_0\n");
			if (ctrller_type == CANFD_BASIC_FRAME)
				ret = can_loopback_can_basic(CANFD0_BASE_ADDR,
							CANFD0_BASE_ADDR,
							dlc_type,
							can_test_data,
							CANFD_BASIC_FRAME);
			else if (ctrller_type == CAN_BASIC_FRAME)
				ret = can_loopback_can_basic(CAN0_BASE_ADDR,
							CAN0_BASE_ADDR,
							dlc_type,
							can_test_data,
							CAN_BASIC_FRAME);
			else
				printf("controller type should be 0 or 1\n");
		} else if (tx_dir == 1) {
			/* loopback canfd_1 to canfd_1 */
			printf("loopback controller_1\n");
			if (ctrller_type == CANFD_BASIC_FRAME)
				ret = can_loopback_can_basic(CANFD1_BASE_ADDR,
							CANFD1_BASE_ADDR,
							dlc_type,
							can_test_data,
							CANFD_BASIC_FRAME);
			else if (ctrller_type == CAN_BASIC_FRAME)
				ret = can_loopback_can_basic(CAN1_BASE_ADDR,
							CAN1_BASE_ADDR,
							dlc_type,
							can_test_data,
							CAN_BASIC_FRAME);
			else
				printf("controller type should be 0 or 1\n");
		} else {
			return CMD_RET_USAGE;
		}

		if (ret)
			printf("controller loopback test, failed\n");
		else
			printf("controller loopback test, successed\n");

		/* mode check , loopback different */
		/* When the external channel is disconnected, */
		/* switch to loopback mode and need to read fifo */
		/* controller_0 to controller_1 */
		if (tx_dir == 0)
			canfd0_last_mode_flag = 0;
		else if (tx_dir == 1)
			canfd1_last_mode_flag = 0;
	}

	/* controller A to controller B */
	if (tx_mode == 1) {
		/* controller_0 to controller_1 */
		if (tx_dir == 0) {
			/* canfd_0 to canfd_1 */
			printf("controller_0 tx controller_1\n");
			if (ctrller_type == CANFD_BASIC_FRAME)
				ret = can_tx_to_xtor_can_basic(CANFD0_BASE_ADDR,
							CANFD1_BASE_ADDR,
							dlc_type,
							can_test_data,
							CANFD_BASIC_FRAME);
			else if (ctrller_type == CAN_BASIC_FRAME)
				ret = can_tx_to_xtor_can_basic(CAN0_BASE_ADDR,
							CAN1_BASE_ADDR,
							dlc_type,
							can_test_data,
							CAN_BASIC_FRAME);
			else {
				printf("controller type should be 0 or 1;");
				printf("[0:can, 1:canfd]\n");
			}
		} else if (tx_dir == 1) {
			/* canfd_1 to canfd_0 */
			printf("controller_1 tx controller_0\n");
			if (ctrller_type == CANFD_BASIC_FRAME)
				ret = can_tx_to_xtor_can_basic(CANFD1_BASE_ADDR,
							CANFD0_BASE_ADDR,
							dlc_type,
							can_test_data,
							CANFD_BASIC_FRAME);
			else if (ctrller_type == CAN_BASIC_FRAME)
				ret = can_tx_to_xtor_can_basic(CAN1_BASE_ADDR,
							CAN0_BASE_ADDR,
							dlc_type,
							can_test_data,
							CAN_BASIC_FRAME);
			else {
				printf("controller type should be 0 or 1;");
				printf("[0:can, 1:canfd]\n");
			}
		} else {
			return CMD_RET_USAGE;
		}

		if (ret) {
			/* When the external channel is disconnected, */
			/* switch to loopback mode and need to read fifo */
			/* controller_0 to controller_1 */
			if (tx_dir == 0)
				canfd0_last_mode_flag = 1;
			else if (tx_dir == 1)
				canfd1_last_mode_flag = 1;

			printf("can_A tx data to can_B, failed\n");
		} else
			printf("can_A tx data to can_B, successed\n");
	}

	return 0;
}

/***************************************************/

U_BOOT_CMD(
	canfd,	6,	1,	cmd_canfd,
	"canfd test command",
	"<tx_mode> <tx_dir> - Send data\n"
	"<tx_mode>		- Identifies transmit mode;		[0 : loopback, 1 : A to B]\n"
	"<tx_dir>		- Identifies transmit direction;	[0 : controller_0 to controller_1, 1 : controller_1 to controller_0]\n"
	"{expand}		- [tx_mod] [tx_dir] [ctrller_type] [dlc_type] [data_pattern]\n"
);
