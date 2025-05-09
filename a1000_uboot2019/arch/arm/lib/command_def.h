#ifndef __COMMAND_DEF__
#define __COMMAND_DEF__

/* callout command */
#define CMD_GIC_INIT                   100
#define CMD_IRQ_ID                     101
#define CMD_IRQ_ACK                    102
#define CMD_IRQ_DEA                    103
#define CMD_IRQ_ENABLE                 104
#define CMD_IRQ_DISABLE                105
#define CMD_GICD_PENDING               106
#define CMD_GICD_CLEAR_PENDING         107
#define CMD_GICD_SET_PRIO              108
#define CMD_GICD_ICFGR                 109
#define CMD_GICD_SET_ACTIVE            110
#define CMD_GICD_CLEAR_ACTIVE          111
#define CMD_GICD_SET_ITARGETSR         112
#define CMD_GICD_READ_TYPER            113
#define CMD_GICD_WRITE_SGIR            114
#define CMD_GICD_GET_ADDR              115
#define CMD_GICD_GET_IIDR              116
#define CMD_GICH_READ_LR               117
#define CMD_GICH_WRITE_LR              118
#define CMD_GICH_READ_ELSR             119
#define CMD_GICH_READ_EISR             120
#define CMD_GICH_READ_HCR              121
#define CMD_GICH_READ_VMCR             122
#define CMD_GICH_READ_APR              123
#define CMD_GICH_READ_MISR             124
#define CMD_GICH_WRITE_HCR             125
#define CMD_GICH_WRITE_VMCR            126
#define CMD_GICH_WRITE_APR             127
#define CMD_GIC_IS_SGI                 128
#define CMD_GIC_IS_PRIV                129

#define CMD_UART_INIT                  130
#define CMD_UART_READ                  131
#define CMD_UART_WRITE                 132

#define CMD_GICD_GET_PRIO              133
#define CMD_GICD_GET_ITARGETSR         134
#define CMD_GICD_GET_ICFGR             135

#define CMD_PSCI_FIND_FID              136
#endif
