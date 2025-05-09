#include "sec_def.h"
#include "command_def.h"

#ifndef BIT
#define BIT(nr) (1 << (nr))
#endif

#define IER_DMAE  BIT(7)
#define IER_UUE   BIT(6)
#define IER_NRZE  BIT(5)
#define IER_RTIOE BIT(4)
#define IER_MIE   BIT(3)
#define IER_RLSE  BIT(2)
#define IER_TIE   BIT(1)
#define IER_RAVIE BIT(0)

#define IIR_FIFOES1 BIT(7)
#define IIR_FIFOES0 BIT(6)
#define IIR_TOD     BIT(3)
#define IIR_IID2    BIT(2)
#define IIR_IID1    BIT(1)
#define IIR_IP      BIT(0)

#define FCR_ITL2    BIT(7)
#define FCR_ITL1    BIT(6)
#define FCR_RESETTF BIT(2)
#define FCR_RESETRF BIT(1)
#define FCR_TRFIFOE BIT(0)
#define FCR_ITL_1   0
#define FCR_ITL_8   (FCR_ITL1)
#define FCR_ITL_16  (FCR_ITL2)
#define FCR_ITL_32  (FCR_ITL2 | FCR_ITL1)

#define LCR_DLAB  BIT(7)
#define LCR_SB    BIT(6)
#define LCR_STKYP BIT(5)
#define LCR_EPS   BIT(4)
#define LCR_PEN   BIT(3)
#define LCR_STB   BIT(2)
#define LCR_WLS1  BIT(1)
#define LCR_WLS0  BIT(0)

#define LSR_FIFOE BIT(7)
#define LSR_TEMT  BIT(6)
#define LSR_TDRQ  BIT(5)
#define LSR_BI    BIT(4)
#define LSR_FE    BIT(3)
#define LSR_PE    BIT(2)
#define LSR_OE    BIT(1)
#define LSR_DR    BIT(0)

#define MCR_LOOP BIT(4)
#define MCR_OUT2 BIT(3)
#define MCR_OUT1 BIT(2)
#define MCR_RTS  BIT(1)
#define MCR_DTR  BIT(0)

#define MSR_DCD  BIT(7)
#define MSR_RI   BIT(6)
#define MSR_DSR  BIT(5)
#define MSR_CTS  BIT(4)
#define MSR_DDCD BIT(3)
#define MSR_TERI BIT(2)
#define MSR_DDSR BIT(1)
#define MSR_DCTS BIT(0)

#define USR_RFF  BIT(4)
#define USR_RFNE BIT(3)
#define USR_TFE  BIT(2)
#define USR_TFNF BIT(1)
#define USR_BUSY BIT(0)

#define UART_SRC_CLK        (25000000UL)

struct uart_regs_t {
    union {
        unsigned int rbr; /* 0x0 */
        unsigned int dll;
        unsigned int thr;
    };
    union {
        unsigned int dlh; /* 0x4 */
        unsigned int ier;
    };
    union {
        unsigned int fcr; /* 0x8 */
        unsigned int iir;
    };
    unsigned int lcr;   /* 0xc */
    unsigned int mcr;   /* 0x10 */
    unsigned int lsr;   /* 0x14 */
    unsigned int msr;   /* 0x18 */
    unsigned int scr;   /* 0x1c */
    unsigned int lpdll; /* 0x20 */
    unsigned char  reserved0[0x7c - 0x24];
    unsigned int usr; /* 0x7c */
    unsigned char  reserved1[0xc0 - 0x80];
#define BST_UART_DLF_LEN 6
    unsigned int dlf; /* 0xc0 */
};

CALLOUT void uart_init(volatile struct uart_regs_t *uart)
{
    unsigned int divider = (UART_SRC_CLK << (BST_UART_DLF_LEN - 4)) / (115200);

    uart->ier = 0;                         /* Disable interrupts*/
    uart->fcr = 1;                         /* Enable FIFOs */
    uart->mcr = 0;                          /* Disable flow ctrl */
    uart->mcr = uart->mcr | MCR_RTS;        /* Clear rts */
    uart->lcr = uart->lcr | LCR_DLAB;     /* Enable access DLL & DLH */

    /* Set baud rate */
    uart->dll = (divider >> BST_UART_DLF_LEN) & 0xff;
    uart->dlh = (divider >> (BST_UART_DLF_LEN + 8)) & 0xff;
    uart->dlf = divider & (BIT(BST_UART_DLF_LEN) - 1);

    /* Clear DLAB bit */
    uart->lcr = uart->lcr & (~(unsigned int)LCR_DLAB);

    /* Set data length to 8 bit, 1 stop bit, no parity */
    uart->lcr = uart->lcr | (LCR_WLS1 | LCR_WLS0);
    //uart->ier = 1;
}
CALLOUT_CLASS(uart_init, CMD_UART_INIT);

CALLOUT int uart_read(volatile struct uart_regs_t *uart)
{
    while (!((uart->lsr) & LSR_DR))
        ;
    return uart->rbr & 0xff;
}
CALLOUT_CLASS(uart_read, CMD_UART_READ);

CALLOUT int uart_write(volatile struct uart_regs_t *uart, char c)
{

    while (!((uart->lsr) & LSR_TEMT))
        ;
    uart->thr = c;
    return (int)c;
}
CALLOUT_CLASS(uart_write, CMD_UART_WRITE);
