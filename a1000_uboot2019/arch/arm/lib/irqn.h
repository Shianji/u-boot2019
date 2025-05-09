#ifndef QEMU_IRQN_H
#define QEMU_IRQN_H

/* WARNING: this header is shared between kernel and rootserver, thus DO NOT inclue any kernel headers. */
enum exception_numbers {
    IRQN_SGI0 =
        0,    // SGI 0 Software Generated Interrupt(0~15)，由CPU直接写入对应的寄存器来触发中断，是ARM中的核间通信使用的核间中断。类似于X86中的IPI
    IRQN_SGI1  = 1,     // SGI 1
    IRQN_SGI2  = 2,     // SGI 2
    IRQN_SGI3  = 3,     // SGI 3
    IRQN_SGI4  = 4,     // SGI 4
    IRQN_SGI5  = 5,     // SGI 5
    IRQN_SGI6  = 6,     // SGI 6
    IRQN_SGI7  = 7,     // SGI 7
    IRQN_SGI8  = 8,     // SGI 8
    IRQN_SGI9  = 9,     // SGI 9
    IRQN_SGI10 = 10,    // SGI 10
    IRQN_SGI11 = 11,    // SGI 11
    IRQN_SGI12 = 12,    // SGI 12
    IRQN_SGI13 = 13,    // SGI 13
    IRQN_SGI14 = 14,    // SGI 14
    IRQN_SGI15 = 15,    // SGI 15

    IRQN_PRIVATE_PTIMER_EL2  = 26,
    IRQN_PRIVATE_VTIMER_EL1 = 27,
    IRQN_PRIVATE_PTIMER_EL1 = 30,

    IRQN_UART0             = 33,
    IRQN_TIMER0            = 141,
    IRQN_COUNT,
};

// SGI8-SGI15预留给安全固件
#define NR_IPI          IRQN_SGI8

#endif
