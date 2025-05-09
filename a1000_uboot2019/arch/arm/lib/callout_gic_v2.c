#include "sec_def.h"
#include "irqn.h"
#include "command_def.h"

#define BIT_MASK(OFF, LEN) ((((1ULL<<((LEN)-1))<<1)-1)<<(OFF))
#define bitmask(x)            (1UL << (x))
#define lowbitsmask(x)        (bitmask(x) - 1UL)
/************************** GICV2 **************************/
#define GICH_VTR_ListRegs_OFF (0)
#define GICH_VTR_ListRegs_LEN (6)
#define GICH_VTR_ListRegs_MSK BIT_MASK( GICH_VTR_ListRegs_OFF, GICH_VTR_ListRegs_LEN)
/* CPU Interface Control Register, GICC_CTLR */
#define GICC_CTLR_EN_BIT (0x1)
#define GICC_CTLR_EOImodeNS_BIT (1UL << 9)

/* Hypervisor Control Register, GICH_HCR */
#define GICH_HCR_LRENPIE_BIT (1 << 2)

#define GIC_MAX_SGIS 16
#define GIC_MAX_PPIS 16
#define GIC_CPU_PRIV (GIC_MAX_SGIS + GIC_MAX_PPIS)
/************************** GICV2 end **************************/

/************************** GIC **************************/
#define DEVICE_SIZE_RES    (4 * 1024)
#define DEVICE_SIZE_GICD   (4 * 1024)
#define DEVICE_SIZE_GICC   (0x2000)

#define GICD_OFFSET (DEVICE_SIZE_RES)
#define GICC_OFFSET (DEVICE_SIZE_RES + DEVICE_SIZE_GICD)
#define GICH_OFFSET (DEVICE_SIZE_RES + DEVICE_SIZE_GICD + DEVICE_SIZE_GICC)

#define ERROR_DEPRECATED 0

/* Compute the number of elements in the given array */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))

/****************************************************************************************************************************************************************/
/*																				*/
/*	Interrupt ID for Secure world												*/
/*																				*/
/****************************************************************************************************************************************************************/
#define ARM_IRQ_SEC_TMU_CH0 (168u) /* Secure timer unit ch0                */

#define INTR_TYPE_NS (2u)

/* SCR definitions */
#define SCR_IRQ_BIT (1 << 1)
#define SCR_FIQ_BIT (1 << 2)

/*******************************************************************************
 * GIC Distributor interface general definitions
 ******************************************************************************/
/* Constants to categorise interrupts */
#define MIN_SGI_ID     0
#define MIN_SEC_SGI_ID 8
#define MIN_PPI_ID     16
#define MIN_SPI_ID     32
#define MAX_SPI_ID     1019

#define TOTAL_SPI_INTR_NUM  (MAX_SPI_ID - MIN_SPI_ID + 1)
#define TOTAL_PCPU_INTR_NUM (MIN_SPI_ID - MIN_SGI_ID)

/* Mask for the priority field common to all GIC interfaces */
#define GIC_PRI_MASK 0xff

/* Mask for the configuration field common to all GIC interfaces */
#define GIC_CFG_MASK 0x3

/* Constant to indicate a spurious interrupt in all GIC versions */
#define GIC_SPURIOUS_INTERRUPT 1023

/* Interrupt configurations */
#define GIC_INTR_CFG_LEVEL 0
#define GIC_INTR_CFG_EDGE  1

/* Constants to categorise priorities */
#define GIC_HIGHEST_SEC_PRIORITY 0x0U
#define GIC_LOWEST_SEC_PRIORITY  0x7fU
#define GIC_HIGHEST_NS_PRIORITY  0x80U
#define GIC_LOWEST_NS_PRIORITY   0xfeU /* 0xff would disable all interrupts */

/*******************************************************************************
 * GIC Distributor interface register offsets
 ******************************************************************************/
#define GICD_CTLR       0x0
#define GICD_TYPER      0x4
#define GICD_IIDR       0x8
#define GICD_IGROUPR    0x80
#define GICD_ISENABLER  0x100
#define GICD_ICENABLER  0x180
#define GICD_ISPENDR    0x200
#define GICD_ICPENDR    0x280
#define GICD_ISACTIVER  0x300
#define GICD_ICACTIVER  0x380
#define GICD_IPRIORITYR 0x400
#define GICD_ICFGR      0xc00
#define GICD_NSACR      0xe00

/* GICD_CTLR bit definitions */
#define CTLR_ENABLE_G0_SHIFT 0
#define CTLR_ENABLE_G0_MASK  0x1
#define CTLR_ENABLE_G0_BIT   (1U << CTLR_ENABLE_G0_SHIFT)

/*******************************************************************************
 * GIC Distributor interface register constants
 ******************************************************************************/
#define PIDR2_ARCH_REV_SHIFT 4
#define PIDR2_ARCH_REV_MASK  0xf

/* GICv3 revision as reported by the PIDR2 register */
#define ARCH_REV_GICV3 0x3
/* GICv2 revision as reported by the PIDR2 register */
#define ARCH_REV_GICV2 0x2
/* GICv1 revision as reported by the PIDR2 register */
#define ARCH_REV_GICV1 0x1

#define IGROUPR_SHIFT    5
#define ISENABLER_SHIFT  5
#define ICENABLER_SHIFT  ISENABLER_SHIFT
#define ISPENDR_SHIFT    5
#define ICPENDR_SHIFT    ISPENDR_SHIFT
#define ISACTIVER_SHIFT  5
#define ICACTIVER_SHIFT  ISACTIVER_SHIFT
#define IPRIORITYR_SHIFT 2
#define ITARGETSR_SHIFT  2
#define ICFGR_SHIFT      4
#define NSACR_SHIFT      4

/* GICD_TYPER shifts and masks */
#define TYPER_IT_LINES_NO_SHIFT 0
#define TYPER_IT_LINES_NO_MASK  0x1f

/* Value used to initialize Normal world interrupt priorities four at a time */
#define GICD_IPRIORITYR_DEF_VAL                                                                   \
    (GIC_HIGHEST_NS_PRIORITY | (GIC_HIGHEST_NS_PRIORITY << 8) | (GIC_HIGHEST_NS_PRIORITY << 16) | \
     (GIC_HIGHEST_NS_PRIORITY << 24))

#define GICC_IAR_INTID_WIDTH 10
// gicv2.h

/*******************************************************************************
 * GICv2 miscellaneous definitions
 ******************************************************************************/

/* Interrupt group definitions */
#define GICV2_INTR_GROUP0 0
#define GICV2_INTR_GROUP1 1

/* Interrupt IDs reported by the HPPIR and IAR registers */
#define PENDING_G1_INTID 1022

/* GICv2 can only target up to 8 PEs */
#define GICV2_MAX_TARGET_PE 8

/*******************************************************************************
 * GICv2 specific Distributor interface register offsets and constants.
 ******************************************************************************/
#define GICD_PIDR2_GICV2 0xFE8

#define SGIR_TGTLSTFLT_SHIFT 24
#define SGIR_TGTLSTFLT_MASK  0x3
#define SGIR_TGTLST_SHIFT    16
#define SGIR_TGTLST_MASK     0xff
#define SGIR_INTID_MASK      0xf

#define SGIR_TGT_SPECIFIC 0

#define GICV2_SGIR_VALUE(tgt_lst_flt, tgt, intid)                      \
    ((((tgt_lst_flt) & SGIR_TGTLSTFLT_MASK) << SGIR_TGTLSTFLT_SHIFT) | \
     (((tgt) & SGIR_TGTLST_MASK) << SGIR_TGTLST_SHIFT) | ((intid) & SGIR_INTID_MASK))

/*******************************************************************************
 * GICv2 specific CPU interface register offsets and constants.
 ******************************************************************************/
/* Physical CPU Interface registers */

/* GICC_CTLR bit definitions */

/* GICC_IIDR bit masks and shifts */

/* HYP view virtual CPU Interface registers */

/* Virtual CPU Interface registers */

/* GICD_CTLR bit definitions */
#define CTLR_ENABLE_G1_SHIFT 1
#define CTLR_ENABLE_G1_MASK  0x1
#define CTLR_ENABLE_G1_BIT   (1 << CTLR_ENABLE_G1_SHIFT)

#define GIC400_NUM_SPIS (480u)
#define MAX_PPIS        (14u)
#define MAX_SGIS        (16u)

#define GRP0                (0u)
#define GRP1                (1u)
#define GIC_TARGET_CPU_MASK (0xffu)

#define ENABLE_GRP0 ((1u) << 0)
#define ENABLE_GRP1 ((1u) << 1)

/* Distributor interface definitions */
#define GICD_ITARGETSR (0x800u)
#define GICD_SGIR      (0xF00)
#define GICD_CPENDSGIR (0xF10)
#define GICD_SPENDSGIR (0xF20)

#define CPENDSGIR_SHIFT (2u)
#define SPENDSGIR_SHIFT CPENDSGIR_SHIFT

/* GICD_TYPER bit definitions */
#define IT_LINES_NO_MASK (0x1fu)

/* GICD_SGIR bit definitions */
#define ICDSGIR_BIT_TLF   (24u)
#define ICDSGIR_BIT_CPUTL (16u)
#define ICDSGIR_BIT_SATT  (15u)

/* Physical CPU Interface registers */
#define GICC_CTLR     (0x0u)
#define GICC_PMR      (0x4u)
#define GICC_BPR      (0x8u)
#define GICC_IAR      (0xCu)
#define GICC_EOIR     (0x10u)
#define GICC_RPR      (0x14u)
#define GICC_HPPIR    (0x18u)
#define GICC_AHPPIR   (0x28u)
#define GICC_IIDR     (0xFCu)
#define GICC_DIR      (0x1000u)
#define GICC_PRIODROP GICC_EOIR

/* Common CPU Interface definitions */
#define INT_ID_MASK (0x3ffu)

/* GICC_CTLR bit definitions */
#define EOI_MODE_NS      ((1u) << 10)
#define EOI_MODE_S       ((1u) << 9)
#define IRQ_BYP_DIS_GRP1 ((1u) << 8)
#define FIQ_BYP_DIS_GRP1 ((1u) << 7)
#define IRQ_BYP_DIS_GRP0 ((1u) << 6)
#define FIQ_BYP_DIS_GRP0 ((1u) << 5)
#define CBPR             ((1u) << 4)
#define FIQ_EN           ((1u) << 3)
#define ACK_CTL          ((1u) << 2)
#define FIQ_EN_SHIFT     3
#define FIQ_EN_BIT       (1 << FIQ_EN_SHIFT)
#define FIRQ_DIS_BIT     ~(1U << FIQ_EN_SHIFT)
#define GICC_CTLR_ENABLEGRP1 (0x1u)

/* GICC_IIDR bit masks and shifts */
#define GICC_IIDR_PID_SHIFT  (20u)
#define GICC_IIDR_ARCH_SHIFT (16u)
#define GICC_IIDR_REV_SHIFT  (12u)
#define GICC_IIDR_IMP_SHIFT  (0u)

#define GICC_IIDR_PID_MASK  (0xfffu)
#define GICC_IIDR_ARCH_MASK (0xfu)
#define GICC_IIDR_REV_MASK  (0xfu)
#define GICC_IIDR_IMP_MASK  (0xfffu)

/* HYP view virtual CPU Interface registers */
#define GICH_HCR     (0x0u)
#define GICH_VTR     (0x4u)
#define GICH_VMCR    (0x8u)
#define GICH_MISR    (0x10u)
#define GICH_EISR0    (0x20u)
#define GICH_EISR1    (0x24u)
#define GICH_ELSR0    (0x30u)
#define GICH_ELSR1    (0x34u)
#define GICH_APR     (0xF0u)
#define GICH_LR_BASE (0x100u)

#define GICH_PAD0_OFFSET (0x10u - 0x0cu)
#define GICH_PAD1_OFFSET (0x20u - 0x14u)
#define GICH_PAD2_OFFSET (0x30u - 0x28u)
#define GICH_PAD3_OFFSET (0xf0u - 0x38u)
#define GICH_PAD4_OFFSET (0x100u - 0x0f4u)

/* Virtual CPU Interface registers */
#define GICV_CTL         (0x0u)
#define GICV_PRIMASK     (0x4u)
#define GICV_BP          (0x8u)
#define GICV_INTACK      (0xCu)
#define GICV_EOI         (0x10u)
#define GICV_RUNNINGPRI  (0x14u)
#define GICV_HIGHESTPEND (0x18u)
#define GICV_DEACTIVATE  (0x1000u)

#define LEVEL_TRIGGER (0)
#define EDGE_TIRGGER  (1)

#define SECURITY     (1)
#define NON_SECURITY (0)

/* Create an interrupt property descriptor from various interrupt properties */
#define INTR_PROP_DESC(num, pri, grp, cfg)                                  \
    {                                                                       \
        .intr_num = num, .intr_pri = pri, .intr_grp = grp, .intr_cfg = cfg \
    }

#define ARM_IRQ_GPT           87
#define ARM_IRQ_SEC_TMU_O     136
#define ARM_IRQ_SEC_TMU_CH0_0 168u
#define ARM_IRQ_SEC_TMU_CH0_1 169u
#define ARM_IRQ_SEC_TMU_CH0_2 170u
#define ARM_IRQ_SEC_TMU       166u

#define PLATFORM_G1S_PROPS(grp)                                                                  \
    INTR_PROP_DESC(IRQN_PRIVATE_PTIMER_EL1, GIC_HIGHEST_SEC_PRIORITY, grp, GIC_INTR_CFG_EDGE),        \
        INTR_PROP_DESC(IRQN_UART0, GIC_HIGHEST_SEC_PRIORITY, grp, GIC_INTR_CFG_EDGE),            \
        INTR_PROP_DESC(ARM_IRQ_SEC_TMU_CH0_2, GIC_HIGHEST_SEC_PRIORITY, grp, GIC_INTR_CFG_EDGE), \
        INTR_PROP_DESC(ARM_IRQ_SEC_TMU, GIC_HIGHEST_SEC_PRIORITY, grp, GIC_INTR_CFG_EDGE),       \
        INTR_PROP_DESC(ARM_IRQ_SEC_TMU_O, GIC_HIGHEST_SEC_PRIORITY, grp, GIC_INTR_CFG_EDGE)

#define PLATFORM_G0_PROPS(grp)

typedef struct interrupt_prop {
    unsigned int intr_num : 10;
    unsigned int intr_pri : 8;
    unsigned int intr_grp : 2;
    unsigned int intr_cfg : 2;
} interrupt_prop_t;

CALLOUT_DATA interrupt_prop_t interrupt_props[] = { PLATFORM_G1S_PROPS(GICV2_INTR_GROUP0),
                                                    PLATFORM_G0_PROPS(GICV2_INTR_GROUP0) };
CALLOUT unsigned int current_cpu(void)
{
    unsigned long mpidr;
    asm volatile("mrs %0, mpidr_el1" : "=r" (mpidr) : : "cc");
    return mpidr & 0xff;
}
/**
 * @brief Write a 8-bit value to address
 *
 * @param addr Address to write to
 * @param value Value to write
 */
CALLOUT void mmio_write_8(unsigned long addr, unsigned char value)
{
    *(volatile unsigned char *)addr = value;
}

/**
 * @brief Read a 8-bit value from address
 *
 * @param addr Address to write to
 */
CALLOUT unsigned char mmio_read_8(unsigned long addr)
{
    return *(volatile unsigned char *)addr;
}

/**
 * @brief Write a 16-bit value to address
 *
 * @param addr Address to write to
 * @param value Value to write
 */
CALLOUT void mmio_write_16(unsigned long addr, unsigned short value)
{
    *(volatile unsigned short *)addr = value;
}

/**
 * @brief Read a 16-bit value from address
 *
 * @param addr Address to write to
 */
CALLOUT unsigned short mmio_read_16(unsigned long addr)
{
    return *(volatile unsigned short *)addr;
}

/**
 * @brief Read a 32-bit value from address
 *
 * @param addr Address to write to
 */
CALLOUT unsigned int mmio_read_32(unsigned long addr)
{
    return *(volatile unsigned int *)addr;
}

/**
 * @brief Write a 32-bit value to address
 *
 * @param addr Address to write to
 * @param value Value to write
 */
CALLOUT void mmio_write_32(unsigned long addr, unsigned int value)
{
    *(volatile unsigned int *)addr = value;
}

/*******************************************************************************
 * GIC Distributor interface accessors for writing entire registers
 ******************************************************************************/
CALLOUT unsigned int gicd_get_itargetsr(unsigned long base, unsigned int id)
{
    return mmio_read_8(base + GICD_ITARGETSR + id);
}

CALLOUT void gicd_set_itargetsr(unsigned long base, unsigned int id, unsigned int target)
{
    mmio_write_8(base + GICD_ITARGETSR + id, (target & GIC_TARGET_CPU_MASK));
}

CALLOUT void set_itargetsr(unsigned long base, unsigned int id, unsigned int target)
{
    unsigned long gicd_base = base + GICD_OFFSET;
    mmio_write_8(gicd_base + GICD_ITARGETSR + id, (target & GIC_TARGET_CPU_MASK));
}
CALLOUT_CLASS(set_itargetsr, CMD_GICD_SET_ITARGETSR);

/*******************************************************************************
 * GIC Distributor interface accessors for reading entire registers
 ******************************************************************************/
/**
 * @brief Read the GIC Distributor ctlr register
 *
 * @param base Base address of the GIC Distributor
 */
CALLOUT unsigned int gicd_read_ctlr(unsigned long base)
{
    return mmio_read_32(base + GICD_CTLR);
}

/**
 * @brief Read the GIC Distributor typer register
 *
 * @param base Base address of the GIC Distributor
 */
CALLOUT unsigned int gicd_read_typer(unsigned long base)
{
    return mmio_read_32(base + GICD_TYPER);
}

CALLOUT unsigned int read_typer(unsigned long base)
{
    unsigned long gicd_base = base + GICD_OFFSET;
    return mmio_read_32(gicd_base + GICD_TYPER);
}
CALLOUT_CLASS(read_typer, CMD_GICD_READ_TYPER);

CALLOUT unsigned long gicd_get_addr(unsigned long base)
{
    return (base + GICD_OFFSET);
}
CALLOUT_CLASS(gicd_get_addr, CMD_GICD_GET_ADDR);

/**
 * @brief Read the GIC Distributor PIDR2 register
 *
 * @param base Base address of the GIC Distributor
 */
CALLOUT unsigned int gicd_read_pidr2(unsigned long base)
{
    return mmio_read_32(base + GICD_PIDR2_GICV2);
}

/**
 * @brief Reads a value from igroupr register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_igroupr(unsigned long base, unsigned int id)
{
    unsigned int n = id >> IGROUPR_SHIFT;
    return mmio_read_32(base + GICD_IGROUPR + (n << 2));
}

/**
 * @brief Reads a value from isenabler register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_isenabler(unsigned long base, unsigned int id)
{
    unsigned int n = id >> ISENABLER_SHIFT;
    return mmio_read_32(base + GICD_ISENABLER + (n << 2));
}

/**
 * @brief Reads a value from ispendr register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_ispendr(unsigned long base, unsigned int id)
{
    unsigned int n = id >> ISPENDR_SHIFT;
    return mmio_read_32(base + GICD_ISPENDR + (n << 2));
}

/**
 * @brief Reads a value from iceabler register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_icenabler(unsigned long base, unsigned int id)
{
    unsigned int n = id >> ICENABLER_SHIFT;
    return mmio_read_32(base + GICD_ICENABLER + (n << 2));
}

/**
 * @brief Reads a value from icpendr register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_icpendr(unsigned long base, unsigned int id)
{
    unsigned int n = id >> ICPENDR_SHIFT;
    return mmio_read_32(base + GICD_ICPENDR + (n << 2));
}

/**
 * @brief Reads a value from isactiver register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_isactiver(unsigned long base, unsigned int id)
{
    unsigned int n = id >> ISACTIVER_SHIFT;
    return mmio_read_32(base + GICD_ISACTIVER + (n << 2));
}

/**
 * @brief Reads a value from icactiver register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_icactiver(unsigned long base, unsigned int id)
{
    unsigned int n = id >> ICACTIVER_SHIFT;
    return mmio_read_32(base + GICD_ICACTIVER + (n << 2));
}

/**
 * @brief Reads a value from ipriorityr register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_ipriorityr(unsigned long base, unsigned int id)
{
    unsigned int n = id >> IPRIORITYR_SHIFT;
    return mmio_read_32(base + GICD_IPRIORITYR + (n << 2));
}

/**
 * @brief Reads a value from itargetsr register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_itargetsr(unsigned long base, unsigned int id)
{
    unsigned int n = id >> ITARGETSR_SHIFT;
    return mmio_read_32(base + GICD_ITARGETSR + (n << 2));
}

/**
 * @brief Reads a value from icfgr register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_icfgr(unsigned long base, unsigned int id)
{
    unsigned int n = id >> ICFGR_SHIFT;
    return mmio_read_32(base + GICD_ICFGR + (n << 2));
}

/**
 * @brief Reads a value from cpendsgir register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_cpendsgir(unsigned long base, unsigned int id)
{
    unsigned int n = id >> CPENDSGIR_SHIFT;
    return mmio_read_32(base + GICD_CPENDSGIR + (n << 2));
}

/**
 * @brief Reads a value from spendsgir register
 *
 * @param base Base address of GICD
 * @param id index
 */
CALLOUT unsigned int gicd_read_spendsgir(unsigned long base, unsigned int id)
{
    unsigned int n = id >> SPENDSGIR_SHIFT;
    return mmio_read_32(base + GICD_SPENDSGIR + (n << 2));
}

/*******************************************************************************
 * GIC Hypervisor for reading entire registers
 ******************************************************************************/

/**
 * @brief GICH for writing hcr registers
 *
 * @param base GICH base address
 */
CALLOUT unsigned int gich_read_base_hcr(unsigned long base)
{
    return mmio_read_32(base + GICH_HCR);
}
/**
 * @brief GICH for writing hcr registers
 *
 * @param base GIC base address
 */
CALLOUT unsigned int read_base_hcr(unsigned long base)
{
    unsigned long gich_base = base + GICH_OFFSET;
    return mmio_read_32(gich_base + GICH_HCR);
}
CALLOUT_CLASS(read_base_hcr, CMD_GICH_READ_HCR);

/**
 * @brief GICH for reading vmcr registers
 *
 * @param base GICH base address
 */
CALLOUT unsigned int gich_read_base_vmcr(unsigned long base)
{
    unsigned long gich_base = base + GICH_OFFSET;
    return mmio_read_32(gich_base + GICH_VMCR);
}
CALLOUT_CLASS(gich_read_base_vmcr, CMD_GICH_READ_VMCR);

/**
 * @brief GICH for reading vtr registers
 *
 * @param base GICH base address
 */
CALLOUT unsigned int gich_read_base_vtr(unsigned long base)
{
    return mmio_read_32(base + GICH_VTR);
}

/**
 * @brief GICH for reading apr registers
 *
 * @param base GIC base address
 */
CALLOUT unsigned int gich_read_base_apr(unsigned long base)
{
    unsigned long gich_base = base + GICH_OFFSET;
    return mmio_read_32(gich_base + GICH_APR);
}
CALLOUT_CLASS(gich_read_base_apr, CMD_GICH_READ_APR);

/**
 * @brief GICH for reading misr registers
 *
 * @param base GIC base address
 */
CALLOUT unsigned int gich_read_base_misr(unsigned long base)
{
    unsigned long gich_base = base + GICH_OFFSET;
    return mmio_read_32(gich_base + GICH_MISR);
}
CALLOUT_CLASS(gich_read_base_misr, CMD_GICH_READ_MISR);

/**
 * @brief GICH for reading eisr0 registers
 *
 * @param base GICH base address
 */
CALLOUT unsigned int gich_read_base_eisr0(unsigned long base)
{
    return mmio_read_32(base + GICH_EISR0);
}

/**
 * @brief GICH for reading eisr1 registers
 *
 * @param base GICH base address
 */
CALLOUT unsigned int gich_read_base_eisr1(unsigned long base)
{
    return mmio_read_32(base + GICH_EISR1);
}

/**
 * @brief GICH for reading elsr0 registers
 *
 * @param base GICH base address
 */
CALLOUT unsigned int gich_read_base_elsr0(unsigned long base)
{
    return mmio_read_32(base + GICH_ELSR0);
}

/**
 * @brief GICH for reading elsr1 registers
 *
 * @param base GICH base address
 */
CALLOUT unsigned int gich_read_base_elsr1(unsigned long base)
{
    return mmio_read_32(base + GICH_ELSR1);
}

/**
 * @brief GICH read functions
 *
 * @param base GIC base address
 * @param index index of the register
 */
CALLOUT unsigned int gich_read_base_lr(unsigned long base, unsigned int index)
{
    unsigned long gich_base = base + GICH_OFFSET;
    return mmio_read_32(gich_base + GICH_LR_BASE + (index * sizeof(unsigned int)));
}
CALLOUT_CLASS(gich_read_base_lr, CMD_GICH_READ_LR);

/*******************************************************************************
 * GIC Distributor interface accessors for writing entire registers
 ******************************************************************************/
CALLOUT void gicd_write_ctlr(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICD_CTLR, val);
}
CALLOUT void gicd_write_sgir(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICD_SGIR, val);
}

/**
 * @brief Writes a value to igroupr register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_igroupr(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> IGROUPR_SHIFT;
    mmio_write_32(base + GICD_IGROUPR + (n << 2), val);
}

/**
 * @brief Writes a value to isenabler register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_isenabler(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> ISENABLER_SHIFT;
    mmio_write_32(base + GICD_ISENABLER + (n << 2), val);
}

/**
 * @brief Writes a value to icenabler register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_icenabler(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> ICENABLER_SHIFT;
    mmio_write_32(base + GICD_ICENABLER + (n << 2), val);
}
/**
 * @brief Writes a value to ispendr register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_ispendr(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> ISPENDR_SHIFT;
    mmio_write_32(base + GICD_ISPENDR + (n << 2), val);
}

/**
 * @brief Writes a value to icpendr register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_icpendr(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> ICPENDR_SHIFT;
    mmio_write_32(base + GICD_ICPENDR + (n << 2), val);
}

/**
 * @brief Writes a value to isactiver register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_isactiver(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> ISACTIVER_SHIFT;
    mmio_write_32(base + GICD_ISACTIVER + (n << 2), val);
}

/**
 * @brief Writes a value to icactiver register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_icactiver(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> ICACTIVER_SHIFT;
    mmio_write_32(base + GICD_ICACTIVER + (n << 2), val);
}

/**
 * @brief Writes a value to ipriorityr register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_ipriorityr(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> IPRIORITYR_SHIFT;
    mmio_write_32(base + GICD_IPRIORITYR + (n << 2), val);
}

/**
 * @brief Writes a value to itargetsr register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_itargetsr(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> ITARGETSR_SHIFT;
    mmio_write_32(base + GICD_ITARGETSR + (n << 2), val);
}

/**
 * @brief Writes a value to icfgr register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_icfgr(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> ICFGR_SHIFT;
    mmio_write_32(base + GICD_ICFGR + (n << 2), val);
}

/**
 * @brief Writes a value to cpendsgir register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_cpendsgir(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> CPENDSGIR_SHIFT;
    mmio_write_32(base + GICD_CPENDSGIR + (n << 2), val);
}

/**
 * @brief Writes a value to spendsgir register
 *
 * @param base Base address of GICD
 * @param id Interrupt ID
 * @param val Value to write
 */
CALLOUT void gicd_write_spendsgir(unsigned long base, unsigned int id, unsigned int val)
{
    unsigned int n = id >> SPENDSGIR_SHIFT;
    mmio_write_32(base + GICD_SPENDSGIR + (n << 2), val);
}

/*******************************************************************************
 * GIC Distributor interface accessors for individual interrupt manipulation
 ******************************************************************************/
CALLOUT unsigned int gicd_get_iidr(unsigned long base)
{
    unsigned long gicd_base = base + GICD_OFFSET;
    return mmio_read_32(gicd_base + GICD_IIDR);
}
CALLOUT_CLASS(gicd_get_iidr, CMD_GICD_GET_IIDR);

CALLOUT unsigned int gicd_get_igroupr(unsigned long base, unsigned int id)
{
    unsigned int     bit_num = id & ((1 << IGROUPR_SHIFT) - 1);
    unsigned int reg_val = gicd_read_igroupr(base, id);

    return (reg_val >> bit_num) & 0x1;
}

/**
 * @brief Set the current CPU bit mask from GICD_ITARGETSR0
 *
 * @param base GIC Distributor base address
 * @param id Interrupt ID
 */
CALLOUT void gicd_set_igroupr(unsigned long base, unsigned int id)
{
    unsigned int     bit_num = id & ((1 << IGROUPR_SHIFT) - 1);
    unsigned int reg_val = gicd_read_igroupr(base, id);

    gicd_write_igroupr(base, id, reg_val | (unsigned int)(1 << bit_num));
}

/**
 * @brief  Clear the current CPU bit mask from GICD_ITARGETSR0
 *
 * @param base GIC Distributor base address
 * @param id    Interrupt ID
 */
CALLOUT void gicd_clr_igroupr(unsigned long base, unsigned int id)
{
    unsigned int     bit_num = id & ((1 << IGROUPR_SHIFT) - 1);
    unsigned int reg_val = gicd_read_igroupr(base, id);

    gicd_write_igroupr(base, id, reg_val & ~((unsigned int)(1 << bit_num)));
}

/**
 * @brief Set the value of the isenabler register
 *
 * @param   base    GICD base address
 * @param   id      Interrupt ID
 */
CALLOUT void gicd_set_isenabler(unsigned long base, unsigned int id)
{
    unsigned int bit_num = id & ((1 << ISENABLER_SHIFT) - 1);

    gicd_write_isenabler(base, id, (1 << bit_num));
}

/**
 * @brief Set the value of the icenabler register
 *
 * @param   base    GICD bas
 * @param   id      Interrupt ID
 */
CALLOUT void gicd_set_icenabler(unsigned long base, unsigned int id)
{
    unsigned int bit_num = id & ((1 << ICENABLER_SHIFT) - 1);

    gicd_write_icenabler(base, id, (1 << bit_num));
}

/**
 * @brief Set the value of the ispendr register
 *
 * @param   base    GIC base addr
 * @param   id      Interrupt ID
 */
CALLOUT void gicd_set_ispendr(unsigned long base, unsigned int id)
{
    unsigned int bit_num = id & ((1 << ISPENDR_SHIFT) - 1);
    unsigned long gicd_base = base + GICD_OFFSET;
    gicd_write_ispendr(gicd_base, id, (1 << bit_num));
}
CALLOUT_CLASS(gicd_set_ispendr, CMD_GICD_PENDING);

/**
 * @brief Set the value of the icpendr register
 *
 * @param   base    GICD base address
 * @param   id      Interrupt ID
 */
CALLOUT void gicd_set_icpendr(unsigned long base, unsigned int id)
{
    unsigned int bit_num = id & ((1 << ICPENDR_SHIFT) - 1);

    gicd_write_icpendr(base, id, (1 << bit_num));
}

/**
 * @brief Set the value of the icpendr register
 *
 * @param   base    GIC base address
 * @param   id      Interrupt ID
 */
CALLOUT void set_icpendr(unsigned long base, unsigned int id)
{
    unsigned int bit_num = id & ((1 << ICPENDR_SHIFT) - 1);
    unsigned long gicd_base = base + GICD_OFFSET;
    gicd_write_icpendr(gicd_base, id, (1 << bit_num));
}
CALLOUT_CLASS(set_icpendr, CMD_GICD_CLEAR_PENDING);

/**
 * @brief Set the value of the isactiver register
 *
 * @param   base    GIC base address
 * @param   id      Interrupt ID
 */
CALLOUT void gicd_set_isactiver(unsigned long base, unsigned int id)
{
    unsigned int bit_num = id & ((1 << ISACTIVER_SHIFT) - 1);
    unsigned long gicd_base = base + GICD_OFFSET;
    gicd_write_isactiver(gicd_base, id, (1 << bit_num));
}
CALLOUT_CLASS(gicd_set_isactiver, CMD_GICD_SET_ACTIVE);

/**
 * @brief Set the value of the icactiver register
 *
 * @param   base    GIC base address
 * @param    id    Interrupt ID
 */
CALLOUT void gicd_set_icactiver(unsigned long base, unsigned int id)
{
    unsigned int bit_num = id & ((1 << ICACTIVER_SHIFT) - 1);
    unsigned long gicd_base = base + GICD_OFFSET;
    gicd_write_icactiver(gicd_base, id, (1 << bit_num));
}
CALLOUT_CLASS(gicd_set_icactiver, CMD_GICD_CLEAR_ACTIVE);
/*******************************************************************************
 * GIC CPU interface accessors for writing entire registers
 ******************************************************************************/
/**
 * @brief GICC for writing ctlr registers
 *
 * @param base GICC base address
 * @param val value to write
 */
CALLOUT void gicc_write_ctlr(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICC_CTLR, val);
}

/**
 * @brief GICC for writing PMR registers
 *
 * @param base GICC base address
 * @param val value to write
 */
CALLOUT void gicc_write_pmr(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICC_PMR, val);
}

/**
 * @brief GICC for writing BPR registers
 *
 * @param base GICC base address
 * @param val value to write
 */
CALLOUT void gicc_write_BPR(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICC_BPR, val);
}

/**
 * @brief GICC for writing IAR registers
 *
 * @param base GICC base address
 * @param val value to write
 */
CALLOUT void gicc_write_IAR(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICC_IAR, val);
}

/**
 * @brief GICC for writing RPR registers
 *
 * @param base GICC base address
 * @param val value to write
 */
CALLOUT void gicc_write_RPR(unsigned long base, unsigned int val)
{
     mmio_write_32(base + GICC_RPR, val);
}

/**
 * @brief GICC for writing EOI registers
 *
 * @param base GICC base address
 * @param val value to write
 */
CALLOUT void gicc_write_EOIR(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICC_EOIR, val);
}

/**
 * @brief GICC for writing HPPIR registers
 *
 * @param base GICC base address
 * @param val value to write
 */
CALLOUT void gicc_write_hppir(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICC_HPPIR, val);
}

/**
 * @brief GICC for writing dir registers
 *
 * @param base GICC base address
 * @param val value to write
 */
CALLOUT void gicc_write_dir(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICC_DIR, val);
}
/*******************************************************************************
 * GIC CPU interface accessors for reading entire registers
 ******************************************************************************/
/**
 * @brief GICC for reading IAR registers
 *
 * @param base GICC base address
 */
CALLOUT unsigned int gicc_read_IAR(unsigned long base)
{
    return mmio_read_32(base + GICC_IAR);
}

/**
 * @brief GICC for reading EOIR registers
 *
 * @param base GICC base address
 */
CALLOUT unsigned int gicc_read_EOIR(unsigned long base)
{
    return mmio_read_32(base + GICC_EOIR);
}

/**
 * @brief GICC for reading hppir registers
 *
 * @param base GICC base address
 */
CALLOUT unsigned int gicc_read_hppir(unsigned long base)
{
    return mmio_read_32(base + GICC_HPPIR);
}

/**
 * @brief GICC for reading ahppir registers
 *
 * @param base GICC base address
 */
CALLOUT unsigned int gicc_read_ahppir(unsigned long base)
{
    return mmio_read_32(base + GICC_AHPPIR);
}

/**
 * @brief GICC for reading dir registers
 *
 * @param base GICC base address
 */
CALLOUT unsigned int gicc_read_dir(unsigned long base)
{
    return mmio_read_32(base + GICC_DIR);
}

/**
 * @brief GICC for reading iidr registers
 *
 * @param base GICC base address
 */
CALLOUT unsigned int gicc_read_iidr(unsigned long base)
{
    return mmio_read_32(base + GICC_IIDR);
}

/**
 * @brief GICC for reading rpr registers
 *
 * @param base GICC base address
 */
CALLOUT unsigned int gicc_read_rpr(unsigned long base)
{
    return mmio_read_32(base + GICC_RPR);
}

/*******************************************************************************
 * GICH
 ******************************************************************************/
CALLOUT void gich_write_base_hcr(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICH_HCR, val);
}

CALLOUT void write_base_hcr(unsigned long base, unsigned int val)
{
    unsigned long gich_base = base + GICH_OFFSET;
    mmio_write_32(gich_base + GICH_HCR, val);
}
CALLOUT_CLASS(write_base_hcr, CMD_GICH_WRITE_HCR);

/**
 * @brief Write the base VMCR register
 *
 * @param base Base address of the GIC
 * @param val Value
 */
CALLOUT void gich_write_base_vmcr(unsigned long base, unsigned int val)
{
    unsigned long gich_base = base + GICH_OFFSET;
    mmio_write_32(gich_base + GICH_VMCR, val);
}
CALLOUT_CLASS(gich_write_base_vmcr, CMD_GICH_WRITE_VMCR);

/**
 * @brief Write the base VTR register
 *
 * @param base Base address of the GICH
 * @param val Value
 */
CALLOUT void gich_write_base_vtr(unsigned long base, unsigned int val)
{
    mmio_write_32(base + GICH_VTR, val);
}

/**
 * @brief Write the base APR register
 *
 * @param base Base address of the GIC
 * @param val Value
 */
CALLOUT void gich_write_base_apr(unsigned long base, unsigned int val)
{
    unsigned long gich_base = base + GICH_OFFSET;
    mmio_write_32(gich_base + GICH_APR, val);
}
CALLOUT_CLASS(gich_write_base_apr, CMD_GICH_WRITE_APR);

/**
 * @brief Write the base LR register
 *
 * @param base Base address of the GICH
 * @param index Index of the LR register
 * @param val Value to write
 */
CALLOUT void gich_write_base_lr(unsigned long base, unsigned int index, unsigned int val)
{
    mmio_write_32(base + GICH_LR_BASE + (index * sizeof(unsigned int)), val);
}

/**
 * @brief Write the base LR register
 *
 * @param base Base address of the GIC
 * @param index Index of the LR register
 * @param val Value to write
 */
CALLOUT void write_base_lr(unsigned long base, unsigned int index, unsigned int val)
{
    unsigned long gich_base = base + GICH_OFFSET;
    mmio_write_32(gich_base + GICH_LR_BASE + (index * sizeof(unsigned int)), val);
}
CALLOUT_CLASS(write_base_lr, CMD_GICH_WRITE_LR);

/*******************************************************************************
 * GIC Hypervisor accessors for individual interrupt manipulation
 ******************************************************************************/
/**
 * @brief Write the interrupt list register
 *
 * @param i List register index
 * @param val Value to write
 */
CALLOUT void gicv2_int_write_lr(unsigned long gich_base, unsigned long lrs, unsigned long i, unsigned long val)
{
    if (i < lrs) {
        gich_write_base_lr(gich_base, i, val);
    }
}

/*******************************************************************************
 * Get the current CPU bit mask from GICD_ITARGETSR0
 ******************************************************************************/
CALLOUT unsigned int gicv2_get_cpuif_id(unsigned long base)
{
    unsigned int val;

    val = gicd_read_itargetsr(base, 0);
    return val & GIC_TARGET_CPU_MASK;
}

/*******************************************************************************
 * GIC Hypervisor accessors for individual interrupt manipulation
 ******************************************************************************/
/**
 * @brief Read the EISR Register
 * @param base Base address of the GIC
 * @param lrs LR register number
 */
CALLOUT unsigned long gic_get_eisr(unsigned long base, unsigned long lrs) // End of Interrupt Status Registers
{
    unsigned long gich_base = base + GICH_OFFSET;
    unsigned long eisr = gich_read_base_eisr0(gich_base);
    if (lrs > 32) eisr |= (((unsigned long)gich_read_base_eisr1(gich_base) << 32));  // If there are more than 32 lrs, integrate GICH_EISR0 and GICH_EISR1
    return eisr;
}
CALLOUT_CLASS(gic_get_eisr, CMD_GICH_READ_EISR);

/**
 * @brief Read the EISR Register
 * @param base Base address of the GIC
 * @param lrs LR register number
 */
CALLOUT unsigned long gic_get_elrsr(unsigned long base, unsigned long lrs) //  Empty List Register Status Registers
{
    unsigned long gich_base = base + GICH_OFFSET;
    unsigned long elsr = gich_read_base_elsr0(gich_base);
    if (lrs > 32) elsr |= (((unsigned long)gich_read_base_elsr1(gich_base) << 32));
    return elsr;
}
CALLOUT_CLASS(gic_get_elrsr, CMD_GICH_READ_ELSR);

/**
 * @brief Set the triggering mode of interrupt
 *
 * @param base Base address of GIC Distributor
 * @param id Interrupt ID
 * @param cfg Triggering mode
 */
CALLOUT void gicd_set_icfgr(unsigned long base, unsigned int id, unsigned int cfg)
{
    unsigned int bit_num = id & ((1 << ICFGR_SHIFT) - 1);
    unsigned int reg_val = gicd_read_icfgr(base, id);

    /* Clear the field, and insert required configuration */
    reg_val &= ~(unsigned int)(GIC_CFG_MASK << bit_num);
    reg_val |= ((cfg & GIC_CFG_MASK) << bit_num);

    gicd_write_icfgr(base, id, reg_val);
}
/**
 * @brief Set the interrupt configuration register
 *
 * @param base Base address of the GIC
 * @param irqn Interrupt ID
 * @param cfg Configuration
 */
CALLOUT void set_icfgr(unsigned long base, unsigned int irqn, unsigned int cfg)
{
    unsigned long gicd_base = base + GICD_OFFSET;
    gicd_set_icfgr(gicd_base, irqn, cfg);
}
CALLOUT_CLASS(set_icfgr, CMD_GICD_ICFGR);

/*
 * Make sure that the interrupt's group is set before expecting
 * this function to do its job correctly.
 */
CALLOUT void gicd_set_ipriorityr(unsigned long base, unsigned int id, unsigned int pri)
{
    /*
     * Enforce ARM recommendation to manage priority values such
     * that group1 interrupts always have a lower priority than
     * group0 interrupts.
     * Note, lower numerical values are higher priorities so the comparison
     * checks below are reversed from what might be expected.
     */
    mmio_write_8(base + GICD_IPRIORITYR + id, pri & GIC_PRI_MASK);
}
/**
 * @brief Set the interrupt priority register
 *
 * @param base Base address of the GIC
 * @param irqn Interrupt ID
 * @param prio Priority
 */
CALLOUT void set_ipriorityr(unsigned long base, unsigned int irqn, unsigned int pri)
{
    unsigned long gicd_base = base + GICD_OFFSET;
    mmio_write_8(gicd_base + GICD_IPRIORITYR + irqn, pri & GIC_PRI_MASK);
}
CALLOUT_CLASS(set_ipriorityr, CMD_GICD_SET_PRIO);

CALLOUT unsigned int get_ipriorityr(unsigned long base, unsigned long int_id)
{
    unsigned int reg_index = int_id / 4;
    unsigned int bit_offset = (int_id % 4) * 8;

    unsigned long reg_address = (base + GICD_OFFSET + GICD_IPRIORITYR + (reg_index * 4));
    unsigned int reg_val = mmio_read_32(reg_address);
    unsigned int pri = ((reg_val >> bit_offset) & GIC_PRI_MASK);

    return pri;
}
CALLOUT_CLASS(get_ipriorityr, CMD_GICD_GET_PRIO);

CALLOUT unsigned int read_icfgr(unsigned long base, unsigned int id)
{
    unsigned int icfgr_value;
    unsigned int reg_offset = 0xC00 + (id / 16) * 4;

    icfgr_value = *((volatile unsigned int *)(base + GICD_OFFSET + reg_offset));

    unsigned int bit_pos = (id % 16) * 2 + 1;
    unsigned int interrupt_mode = (icfgr_value >> bit_pos) & 0x1;

    return interrupt_mode;
}
CALLOUT_CLASS(read_icfgr, CMD_GICD_GET_ICFGR);

CALLOUT unsigned int get_itargetsr(unsigned long base, unsigned int id)
{
    return mmio_read_8(base + GICD_OFFSET + GICD_ITARGETSR + id);
}
CALLOUT_CLASS(get_itargetsr, CMD_GICD_GET_ITARGETSR);

/*******************************************************************************
 * Helper function to configure the default attributes of SPIs.
 * SPIS in group1
 ******************************************************************************/
CALLOUT void gicv2_spis_configure_defaults(unsigned long gicd_base)
{
    unsigned int index, num_ints;

    // Get the maximum number of interrupts
    num_ints = gicd_read_typer(gicd_base);
    num_ints &= TYPER_IT_LINES_NO_MASK;
    num_ints = (num_ints + 1) << 5;

    /*
     * Treat all SPIs as G1NS by default. The number of interrupts is
     * calculated as 32 * (IT_LINES + 1). We do 32 at a time.
     * Set all SPI interrupts to group1
     */
    for (index = MIN_SPI_ID; index < num_ints; index += 32) {
        gicd_write_igroupr(gicd_base, index, ~0U);
    }

    /* Setup the default SPI priorities doing four at a time Set the priority of all SPI interrupts */
    for (index = MIN_SPI_ID; index < num_ints; index += 4) {
        gicd_write_ipriorityr(gicd_base, index, GICD_IPRIORITYR_DEF_VAL);
    }

    /* Treat all SPIs as level triggered by default, 16 at a time  Set the properties of all SPI interrupts (i.e. how they are triggered)   */
    for (index = MIN_SPI_ID; index < num_ints; index += 16) {
        gicd_write_icfgr(gicd_base, index, 0);
    }
}

/*******************************************************************************
 * Helper function to configure properties of secure G0 SPIs.
 ******************************************************************************/
CALLOUT void gicv2_secure_spis_configure_props(unsigned long gicd_base)
{
    unsigned int interrupt_props_num = ARRAY_SIZE(interrupt_props);
    unsigned int            i;
    const interrupt_prop_t *prop_desc;

    /* Make sure there's a valid property array */
    for (i = 0; i < interrupt_props_num; i++) {
        prop_desc = &interrupt_props[i];

        if (prop_desc->intr_num < MIN_SPI_ID) {    // If the interrupt number is less than 32, it is not an SPI interrupt
            continue;
        }

        /* Configure this interrupt as a secure interrupt */
        // assert(prop_desc->intr_grp == GICV2_INTR_GROUP0);  // If the interrupt is group0, the interrupt number is written to group0
        gicd_clr_igroupr(gicd_base, prop_desc->intr_num);    // Set the interrupt to group0

        /* Set the priority of this interrupt */
        gicd_set_ipriorityr(gicd_base, prop_desc->intr_num,    // Set the interrupt priority  0
                            prop_desc->intr_pri);

        /* Target the secure interrupts to primary CPU */
        gicd_set_itargetsr(gicd_base, prop_desc->intr_num,    // Sets which cpu handles interrupts
                           gicv2_get_cpuif_id(gicd_base));

        /* Set interrupt configuration */
        gicd_set_icfgr(gicd_base, prop_desc->intr_num,    // Set interrupt trigger mode, level or edge
                       prop_desc->intr_cfg);

        /* Enable this interrupt */
        gicd_set_isenabler(gicd_base, prop_desc->intr_num);    // Enable interrupt
    }
}

/*******************************************************************************
 * Helper function to configure properties of secure G0 SGIs and PPIs.
 ******************************************************************************/
CALLOUT void gicv2_secure_ppi_sgi_setup_props(unsigned long gicd_base)
{
    unsigned int interrupt_props_num = ARRAY_SIZE(interrupt_props);
    unsigned int            i;
    unsigned int                sec_ppi_sgi_mask = 0;
    const interrupt_prop_t *prop_desc;

    /*
     * Disable all SGIs (imp. def.)/PPIs before configuring them. This is a
     * more scalable approach as it avoids clearing the enable bits in the
     * GICD_CTLR.
     */
    gicd_write_icenabler(gicd_base, 0, ~0U);

    /* Setup the default PPI/SGI priorities doing four at a time */
    for (i = 0; i < MIN_SPI_ID; i += 4) {    // Set the PPI/SGI default interrupt priority
        gicd_write_ipriorityr(gicd_base, i, GICD_IPRIORITYR_DEF_VAL);
    }
    for (i = 0; i < interrupt_props_num; i++) {
        prop_desc = &interrupt_props[i];

        if (prop_desc->intr_num >= MIN_SPI_ID) {
            continue;
        }
        /*
         * Set interrupt configuration for PPIs. Configuration for SGIs
         * are ignored.
         */
        if ((prop_desc->intr_num >= MIN_PPI_ID) && (prop_desc->intr_num < MIN_SPI_ID)) {
            gicd_set_icfgr(gicd_base, prop_desc->intr_num, prop_desc->intr_cfg);
        }

        /* We have an SGI or a PPI. They are Group0 at reset */
        sec_ppi_sgi_mask |= (1u << prop_desc->intr_num);

        /* Set the priority of this interrupt */
        gicd_set_ipriorityr(gicd_base, prop_desc->intr_num, prop_desc->intr_pri);
    }

    /*
     * Invert the bitmask to create a mask for non-secure PPIs and SGIs.
     * Program the GICD_IGROUPR0 with this bit mask.
     */
    gicd_write_igroupr(gicd_base, 0, ~sec_ppi_sgi_mask);    // Set the SGI and PPI to group1

    /* Enable the Group 0 SGIs and PPIs */
    gicd_write_isenabler(gicd_base, 0, sec_ppi_sgi_mask);    // Set the interrupt function. The interrupt function cannot be enabled because it does not exist
}
/**
 * @brief: gic distributor driver init
 *
 * @param gicd_base Base address of GIC Distributor
 */
CALLOUT void gicd_driver_init(unsigned long gicd_base)
{
    /**************** plat_arm_gic_init *******************/
    unsigned int         ctlr;

    /* Disable the distributor before going further */
    ctlr = gicd_read_ctlr(gicd_base);
    gicd_write_ctlr(gicd_base, ctlr & ~(CTLR_ENABLE_G0_BIT | CTLR_ENABLE_G1_BIT));    // disable group0 和 group1

    /* Set the default attribute of all SPIs */
    gicv2_spis_configure_defaults(gicd_base);    // Set SPI interrupt default handling mode Default group1, level trigger, priority 0x0f

    gicv2_secure_spis_configure_props(gicd_base);    // Registration was interrupted to group0

    /* Re-enable the secure SPIs now that they have been configured */
    gicd_write_ctlr(gicd_base, ctlr | CTLR_ENABLE_G0_BIT);    // Example Interrupting group0 was enabled

    gicv2_secure_ppi_sgi_setup_props(gicd_base);    // Set the PPI and SGI

    /* Enable G0 interrupts if not already */
    ctlr = gicd_read_ctlr(gicd_base);

    if ((ctlr & CTLR_ENABLE_G0_BIT) == 0) {
        gicd_write_ctlr(gicd_base, ctlr | CTLR_ENABLE_G0_BIT);
    }
}

/**
 * @brief   Disable GICD
 *
 * @param gicd_base Base address of GIC Distributor
 */
CALLOUT void gicd_disable(unsigned long gicd_base)
{
    unsigned int gicd_ctrl = gicd_read_ctlr(gicd_base);
    gicd_ctrl &= ~(CTLR_ENABLE_G0_BIT | CTLR_ENABLE_G1_BIT);
    gicd_write_ctlr(gicd_base, gicd_ctrl);
}

/**
 * @brief Set the interrupt disable register
 *
 * @param gicd_base Base address of GIC Distributor
 * @param irqn Interrupt ID
 */
CALLOUT void gicv2_int_disable(unsigned long gicd_base, unsigned int irqn)
{
    gicd_set_icenabler(gicd_base, irqn);
}

/**
 * @brief Set the interrupt enable register
 *
 * @param gicd_base Base address of GIC Distributor
 * @param irqn Interrupt ID
 */
CALLOUT void gicv2_int_enable(unsigned long gicd_base, unsigned int irqn)
{
    gicd_set_isenabler(gicd_base, irqn);
}

/**
 * @brief Clear the interrupt pending register
 *
 * @param gicd_base Base address of GIC Distributor
 * @param irqn Interrupt ID
 */
CALLOUT void gicv2_int_clear_pending(unsigned long gicd_base, unsigned int irqn)
{
    gicd_set_icpendr(gicd_base, irqn);
}

/**
 * @brief   Configure interrupt
 *
 * @param   gicd_base  Base address of GICD
 * @param   irqn       Interrupt number
 * @param   trigger    Interrupt trigger type
 * @param   cpus       Affinity mask
 * @param   priority   Interrupt priority
 * @param   security   Security level
 */
CALLOUT void gicv2_int_config(unsigned long gicd_base, 
								unsigned int irqn, 
								unsigned int trigger, 
								unsigned int cpus, 
								unsigned int priority, 
								unsigned int security)
{
    // SGI Int_config fields are read-only
    if (irqn >= 16) {
        gicd_set_icfgr(gicd_base, irqn, trigger);
    }

    // GICD_ITARGETSR0 to GICD_ITARGETSR7 are read-only
    if (irqn >= 32) {
        gicd_set_itargetsr(gicd_base, irqn, cpus);
    }

    gicd_set_ipriorityr(gicd_base, irqn, priority);

    if (security) {
        gicd_clr_igroupr(gicd_base, irqn);    // group 0 , security
    } else {
        gicd_set_igroupr(gicd_base, irqn);    // group 1 , non-security
    }
}

/**
 * @brief   Enable GICD
 * 
 * @param gicd_base Base address of GIC Distributor
 */
CALLOUT void gicd_enable(unsigned long gicd_base)
{
    unsigned int gicd_ctrl = gicd_read_ctlr(gicd_base);
    gicd_ctrl |= CTLR_ENABLE_G0_BIT;    // only enable group0
    gicd_write_ctlr(gicd_base, gicd_ctrl);
}

/**
 * @brief SPI default config
 *
 * @param gicd_base  Base address of GICD
 * @param start start irqn
 * @param end    end irqn
 * @param cpus   target cpu
 */
CALLOUT void gicd_int_default_config(unsigned long gicd_base, unsigned int start, unsigned int end, unsigned int cpus)
{
    unsigned int irqn;
    for (irqn = start; irqn < end; irqn++) {
        gicv2_int_disable(gicd_base, irqn);
        gicv2_int_clear_pending(gicd_base, irqn);
        gicv2_int_config(gicd_base, irqn, LEVEL_TRIGGER, cpus, 0x20, NON_SECURITY);
    }
}

/**
 * @brief SGI/PPI default config
 *
 * @param gicd_base  Base address of GICD
 * @param start start irqn
 * @param end    end irqn
 * @param priority priority
 */
CALLOUT void gicd_sgi_ppi_default_config(unsigned long gicd_base, unsigned int start, unsigned int end, unsigned int priority)
{
    unsigned int irqn;
    for (irqn = start; irqn < end; irqn++) {
        gicv2_int_disable(gicd_base, irqn);
        gicv2_int_clear_pending(gicd_base, irqn);
        gicv2_int_config(gicd_base, irqn, LEVEL_TRIGGER, 0x0, priority, NON_SECURITY);
    }
}
/**
 * @brief Initialize GICD
 * 
 * @param gicd_base Base address of GIC Distributor
 */
CALLOUT void gicd_init(unsigned long gicd_base)
{
    gicd_driver_init(gicd_base);
    gicd_disable(gicd_base);
    gicd_int_default_config(gicd_base, 32, 287, 0x1);    // spi
    gicd_sgi_ppi_default_config(gicd_base, 0, 16, 0x10);              // sgi
    gicd_sgi_ppi_default_config(gicd_base, 16, 32, 0x20);             // ppi
    gicd_enable(gicd_base);
}

/**
 * @brief Initialize the GICv2 driver
 * 
 * @param gicd_base Base address of GIC Distributor
 */
CALLOUT void gicd_pri_driver_init(unsigned long gicd_base)
{
	gicv2_secure_ppi_sgi_setup_props(gicd_base);    // Set the PPI and SGI
}
/**
 * @brief Initialize GICC
 * 
 * @param gicc_base GICC base address
 */
CALLOUT void gicc_init(unsigned long gicc_base)
{
    unsigned int val;
    /*
     * Enable the Group 0 interrupts, FIQEn and disable Group 0/1
     * bypass.
     */
    val = (GICC_CTLR_ENABLEGRP1 | FIQ_BYP_DIS_GRP0) & FIRQ_DIS_BIT;
    val |= IRQ_BYP_DIS_GRP0 | FIQ_BYP_DIS_GRP1 | IRQ_BYP_DIS_GRP1 | GICC_CTLR_EOImodeNS_BIT;

    /* Program the idle priority in the PMR */
    gicc_write_pmr(gicc_base, GIC_PRI_MASK);    // 优先级越大 数值越小
    gicc_write_ctlr(gicc_base, val);            // Enable CPU interface

}

/**
 * @brief Initialize GICH
 * 
 * @param gich_base GICC base address
 */
CALLOUT unsigned long gich_init(unsigned long gich_base)
{
	unsigned long num_lrs = ((gich_read_base_vtr(gich_base) & GICH_VTR_ListRegs_MSK) >>  GICH_VTR_ListRegs_OFF) + 1;

    /*Initialise LR*/
    for (unsigned long i = 0; i < num_lrs; i++) {
        gicv2_int_write_lr(gich_base, num_lrs, i, 0);
    }
    /*A maintenance interrupt is asserted while the EOICount field is not 0*/
    gich_write_base_hcr(gich_base, gich_read_base_hcr(gich_base) \
                                        | GICH_HCR_LRENPIE_BIT);
	
	return num_lrs;
}

/**
 * @brief Initialization belongs to the part of the GIC that each core should initialize
 *
 * @param gic_base  Base address of GIC
 */
CALLOUT unsigned long gic_cpu_init(unsigned long gic_base)
{
    unsigned long gicd_base = gic_base + GICD_OFFSET;
    unsigned long gicc_base = gic_base + GICC_OFFSET;
    unsigned long gich_base = gic_base + GICH_OFFSET;

    gicd_sgi_ppi_default_config(gicd_base, 0, 16, 0x10);              // sgi
    gicd_sgi_ppi_default_config(gicd_base, 16, 32, 0x20);             // ppi

    gicc_init(gicc_base);

    return gich_init(gich_base);
}
/**
 * @brief   GIC CPU init
 *
 * @param gic_base  Base address of GIC
 */
CALLOUT unsigned long interrupt_gic_init(unsigned long gic_base) 
{
    unsigned long gicd_base = gic_base + GICD_OFFSET;
	if (current_cpu() == 0) {
		gicd_init(gicd_base);
	} else {
        gicd_pri_driver_init(gicd_base);
    }

	return gic_cpu_init(gic_base);
}
CALLOUT_CLASS(interrupt_gic_init, CMD_GIC_INIT);

/*******************************************************************************
 * This functions reads the GIC cpu interface Interrupt Acknowledge register
 * to start handling the pending interrupt. It returns the contents of the IAR.
 * This function can read the interrupt number
 ******************************************************************************/
CALLOUT unsigned int interrupt_irq_id(unsigned long gic_base) 
{
    unsigned long gicc_base = gic_base + GICC_OFFSET;
	return (gicc_read_IAR(gicc_base) & lowbitsmask(GICC_IAR_INTID_WIDTH));
}
CALLOUT_CLASS(interrupt_irq_id, CMD_IRQ_ID);

/*******************************************************************************
 * This functions writes the GIC cpu interface End Of Interrupt register with
 * the passed value to finish handling the active interrupt
 * This function needs to be called when the interrupt completes
 ******************************************************************************/
CALLOUT void interrupt_irq_ack(unsigned long gic_base, unsigned int irqn) 
{
    unsigned long gicc_base = gic_base + GICC_OFFSET;
    unsigned int cpu = current_cpu();
    unsigned int val = 0;
    if (irqn < 16) {
        val = cpu << 10 | irqn;
    } else {
        val = irqn;
    }
    gicc_write_EOIR(gicc_base, val);
}
CALLOUT_CLASS(interrupt_irq_ack, CMD_IRQ_ACK);

// After interrupting degradation and deactivation, you need to call this function for deactivation
CALLOUT void interrupt_irq_dea(unsigned long gic_base, unsigned int irqn) 
{
    unsigned long gicc_base = gic_base + GICC_OFFSET;
    unsigned int cpu = current_cpu();
    unsigned int val = 0;
    if (irqn < 16) {
        val = cpu << 10 | irqn;
    } else {
        val = irqn;
    }
    gicc_write_dir(gicc_base, val);
}
CALLOUT_CLASS(interrupt_irq_dea, CMD_IRQ_DEA);

/**
 * @brief Enable interrupt (Keep it temporarily for later use by GICV3)
 *
 * @param gic_base  Base address of GIC
 * @param int_id Interrupt ID
 */
CALLOUT void irq_enable(unsigned long gic_base, unsigned int irqn)
{
    unsigned int bit_num = irqn & ((1 << ISENABLER_SHIFT) - 1);
    unsigned long gicd_base = gic_base + GICD_OFFSET;
    gicd_write_isenabler(gicd_base, irqn, (1 << bit_num));
}
CALLOUT_CLASS(irq_enable, CMD_IRQ_ENABLE);

/**
 * @brief Disable the interrupt (Keep it temporarily for later use by GICV3)
 *
 * @param gic_base  Base address of GIC
 * @param int_id Interrupt ID
 */
CALLOUT void irq_disable(unsigned long gic_base, unsigned int irqn)
{
    unsigned int bit_num = irqn & ((1 << ICENABLER_SHIFT) - 1);
    unsigned long gicd_base = gic_base + GICD_OFFSET;
    gicd_write_icenabler(gicd_base, irqn, (1 << bit_num));
}
CALLOUT_CLASS(irq_disable, CMD_IRQ_DISABLE);

/**
 * @brief   Trigger a SGI interrupt.
 *
 * @param gic_base  Base address of GIC
 * @param   id         SGI ID
 * @param   security   Security level
 * @param   filter     Filter
 * @param   cpulist    CPU list
 */
CALLOUT void interrupt_sgi_trigger(unsigned long gic_base, unsigned int id, unsigned int security, unsigned int filter, unsigned char cpulist)
{
    unsigned int value;
    unsigned long gicd_base = gic_base + GICD_OFFSET;
    if (id >= 16)
        return;    // there is only 16 SGIs. value: [0, 15]
    /* 00指定CPUInterface,
       01除了请求这个中断的处理器之外的所有其他处理器,
       10 中断只发往请求该中断的处理器,
       11 保留 */
    value = filter << ICDSGIR_BIT_TLF;    // cpu列表过滤器

    /* if ICDSGIR_BIT_TLF = 00 指定中断目标处理器  8 bit core 0~7*/
    value |= cpulist << ICDSGIR_BIT_CPUTL;    // cpu列表

    value |= id;    // SGI ID

    if (security == 0) {
        value |= bitmask(ICDSGIR_BIT_SATT);
    }

    gicd_write_sgir(gicd_base, value);

}
CALLOUT_CLASS(interrupt_sgi_trigger, CMD_GICD_WRITE_SGIR);

/**
 * @brief Check if the interrupt is a SGI
 *
 * @param int_id Interrupt ID
 */
CALLOUT unsigned int gic_is_sgi(unsigned long int_id)
{
    return int_id < GIC_MAX_SGIS;
}
CALLOUT_CLASS(gic_is_sgi, CMD_GIC_IS_SGI);

/**
 * @brief Check if the interrupt is a private interrupt
 *
 * @param int_id Interrupt ID
 */
CALLOUT unsigned int gic_is_priv(unsigned long int_id)
{
    return int_id < GIC_CPU_PRIV;
}
CALLOUT_CLASS(gic_is_priv, CMD_GIC_IS_PRIV);
