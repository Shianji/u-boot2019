#ifndef __A1000_ADDR_H
#define __A1000_ADDR_H

#include <boot_info.h>

#define MASK_OF_2M    (0x1FFFFF)
#define SIZE_OF_2M    (0x200000)
/* ***********************************hardware*********************************** */
/* SRAM */
#define SRAM_PA_START (0x1800000)
#define SRAM_PA_SIZE  (0x100000)
/* DDR */
#define DDR0_PA_START (0x80000000)
#define DDR0_PA_SIZE  (0x70000000)
#define DDR1_PA_START (0x180000000)
#define DDR1_PA_SIZE  (0x70000000)
/* in A1000 FAD board, ddr0 has been used as reserved mem for hardware
 * it's unavailable for kernel to use */
#define DDR0_USABLE_PA_START (0)
#define DDR0_USABLE_PA_SIZE  (0)
#define DDR1_USABLE_PA_START (0x180000000)
#define DDR1_USABLE_PA_SIZE  (0x70000000)
/* UART */
#define UART_BASE     (0x20008000)
#define UART_SIZE     (0x1000)
/* GIC */
#define GIC_BASE      (0x32000000)
#define GIC_SIZE      (0x8000)
/* dtb load addr */
#define DTB_A1000_ADDR  (0x8FE00000)
#define DTB_SIZE        (0x20000)

/* ***********************************OS common*********************************** */
/* kernel */
#define KERNEL_IMG_DDR_NO   (1)
#define KERNEL_IMG_OFFSET   (0x63000000)
#define KERNEL_IMG_MAXSIZE  (0x2000000)
/* rootserver */
#define RS_IMG_MAXSIZE      (0x8000000)
#define RS_STACKRES_SIZE    (0x8000)

/* ***********************************OS HYPER*********************************** */
/* empty ddr memory is occupied by hypervisor
 * kernel ddr memory is partly used by microkernel
 */
#define HYPER_DDR_RSVD_MEM_SIZE   (0x50000000)

static inline void bootinfo_init(struct boot_info_t *boot)
{
    int boot_hyper = 0;
    int i = 0;
    const char *ptr;
    u64 ddr_offset = 0;
    u64 kernel_rs_res_len = 0;
    u64 ddr_rest_size = 0;
    unsigned int ddr_kernel_on_no = 0;
    u64 ddr_kernel_on_start = 0;
    u64 ddr_kernel_on_size = 0;
    u64 ddr_free_start = 0;
    u64 ddr_free_size = 0;
    u64 used_size;
#if NON_A1000_PLATFORM
    u64 min_heap_size = ~(0);
    int min_heap_num = -1;
#endif
    memset(boot, 0, sizeof(*boot));

    /* device para init */
    boot->gic_base = GIC_BASE;
    boot->gic_size  = GIC_SIZE;
    boot->uart_base = UART_BASE;
    boot->uart_size = UART_SIZE;

    /* memory para init */
    boot->mem.sram_pa_start       = SRAM_PA_START;
    boot->mem.sram_pa_size        = SRAM_PA_SIZE;
    boot->mem.ddr0_pa_start       = DDR0_PA_START;
    boot->mem.ddr0_pa_size        = DDR0_PA_SIZE;
    boot->mem.ddr1_pa_start       = DDR1_PA_START;
    boot->mem.ddr1_pa_size        = DDR1_PA_SIZE;

    /* kernel memory para init */
    /* force offset to be 2M alignment */
    ddr_offset = KERNEL_IMG_OFFSET & ~(MASK_OF_2M);
    kernel_rs_res_len = (KERNEL_IMG_MAXSIZE + RS_IMG_MAXSIZE + MASK_OF_2M) & ~(MASK_OF_2M);

    /* in A1000 FAD board, ddr0 has been used as reserved mem for hardware
     * it's unavailable for kernel to use */
    ddr_kernel_on_no = KERNEL_IMG_DDR_NO;
    if (ddr_kernel_on_no == 0) {
        ddr_kernel_on_start = DDR0_USABLE_PA_START;
        ddr_kernel_on_size  = DDR0_USABLE_PA_SIZE;
        ddr_free_start      = DDR1_USABLE_PA_START;
        ddr_free_size       = DDR1_USABLE_PA_SIZE;
    } else {
        ddr_kernel_on_start = DDR1_USABLE_PA_START;
        ddr_kernel_on_size  = DDR1_USABLE_PA_SIZE;
        ddr_free_start      = DDR0_USABLE_PA_START;
        ddr_free_size       = DDR0_USABLE_PA_SIZE;
    }

    if (ddr_offset + kernel_rs_res_len > ddr_kernel_on_size) {
        /* correct by set ddr_offset to zero */
        printf("######## ddr_offset is wrong, set value to default 0 ########\n");
        ddr_offset = 0;
    }

    /* get enable_hyp status */
    ptr = env_get("enable_hyp");
    if (!ptr) {
        boot_hyper = 0;
    } else {
        printf("enable hyp val %c\n", *ptr);
        if (*ptr == 0x30) {
            boot_hyper = 0;
        } else if (*ptr == 0x31) {
            boot_hyper = 1;
        }
    }

    ddr_rest_size = ddr_kernel_on_size - kernel_rs_res_len - ddr_offset;
    if (boot_hyper && ddr_rest_size < HYPER_DDR_RSVD_MEM_SIZE && ddr_offset < HYPER_DDR_RSVD_MEM_SIZE) {
        printf("######## rsvd memory must be continuous, set ddr_offset to default 0 ########\n");
        ddr_offset = 0;
        ddr_rest_size = ddr_kernel_on_size - kernel_rs_res_len - ddr_offset;
    }

    boot->kmem.kernel_img_addr    = ddr_kernel_on_start + ddr_offset;
    boot->kmem.kernel_img_maxsize = KERNEL_IMG_MAXSIZE;

    /* rootserver memory para init */
    boot->rsmem.rs_img_addr       = boot->kmem.kernel_img_addr + boot->kmem.kernel_img_maxsize;
    boot->rsmem.rs_img_maxsize    = RS_IMG_MAXSIZE - RS_STACKRES_SIZE;
    boot->rsmem.rs_stackres_addr  = boot->rsmem.rs_img_addr + boot->rsmem.rs_img_maxsize;
    boot->rsmem.rs_stackres_size  = RS_STACKRES_SIZE;

    printf("kernel_img_addr %llx kernel_img_maxsize %x rs_img_addr %llx rs_img_maxsize %x rs_stackres_addr %llx rs_stackres_size %x\n",
        boot->kmem.kernel_img_addr, boot->kmem.kernel_img_maxsize,
        boot->rsmem.rs_img_addr, boot->rsmem.rs_img_maxsize,
        boot->rsmem.rs_stackres_addr, boot->rsmem.rs_stackres_size);

    /* hyper mode heap init */
    if (boot_hyper == 1) {
        /* hyper rsvd mem in front of ddr_kernel_on */
        if (ddr_offset > HYPER_DDR_RSVD_MEM_SIZE) {
            boot->rsmem.heap[0].rs_heap_addr = ddr_kernel_on_start + HYPER_DDR_RSVD_MEM_SIZE;
            boot->rsmem.heap[0].rs_heap_size = ddr_offset - HYPER_DDR_RSVD_MEM_SIZE;
            if (ddr_kernel_on_size > ddr_offset + kernel_rs_res_len) {
                boot->rsmem.heap[1].rs_heap_addr = boot->kmem.kernel_img_addr + kernel_rs_res_len;
                boot->rsmem.heap[1].rs_heap_size = ddr_kernel_on_size - ddr_offset - kernel_rs_res_len;
            }
        } else { /* hyper rsvd mem in end of ddr_kernel_on */
            i = 0;
            if (ddr_offset > 0) {
                boot->rsmem.heap[i].rs_heap_addr = ddr_kernel_on_start;
                boot->rsmem.heap[i].rs_heap_size = ddr_offset;
                ++i;
            }
            used_size = ddr_offset + kernel_rs_res_len + HYPER_DDR_RSVD_MEM_SIZE;
            if (ddr_kernel_on_size > used_size) {
                boot->rsmem.heap[i].rs_heap_addr = boot->kmem.kernel_img_addr + kernel_rs_res_len;
                boot->rsmem.heap[i].rs_heap_size = ddr_kernel_on_size - used_size;
            }
        }
        printf("heap[0] addr 0x%llx size 0x%llx heap[1] addr 0x%llx size 0x%llx\n",
            boot->rsmem.heap[0].rs_heap_addr, boot->rsmem.heap[0].rs_heap_size,
            boot->rsmem.heap[1].rs_heap_addr, boot->rsmem.heap[1].rs_heap_size);
    } else { /* rtos mode heap init */
        i = 0;
        if (ddr_offset) {
            boot->rsmem.heap[i].rs_heap_addr = ddr_kernel_on_start;
            boot->rsmem.heap[i].rs_heap_size = ddr_offset;
            ++i;
        }
        used_size = ddr_offset + kernel_rs_res_len;
        if (used_size < ddr_kernel_on_size) {
            boot->rsmem.heap[i].rs_heap_addr = ddr_kernel_on_start + used_size;
            boot->rsmem.heap[i].rs_heap_size = ddr_kernel_on_size - used_size;
            ++i;
        }
        boot->rsmem.heap[i].rs_heap_addr = ddr_free_start;
        boot->rsmem.heap[i].rs_heap_size = ddr_free_size;
        printf("heap[0] addr 0x%llx size 0x%llx heap[1] addr 0x%llx size 0x%llx heap[2] addr 0x%llx size 0x%llx\n",
            boot->rsmem.heap[0].rs_heap_addr, boot->rsmem.heap[0].rs_heap_size,
            boot->rsmem.heap[1].rs_heap_addr, boot->rsmem.heap[1].rs_heap_size,
            boot->rsmem.heap[2].rs_heap_addr, boot->rsmem.heap[2].rs_heap_size);
    }

    // comment: in A1000 we use specific address because most of ddr0 is reserved for hardware
    // the common way is to find min heap and assign 2M for dtb at right end of the heap
#if NON_A1000_PLATFORM
    for (i = 0; i < BOOTINFO_MAX_HEAP_COUNT; ++i) {
        /* rs_heap_size == 0 means heap invalid, break the circulation */
        if (boot->rsmem.heap[i].rs_heap_size == 0) {
            break;
        }
        if (min_heap_size > boot->rsmem.heap[i].rs_heap_size) {
            min_heap_size = boot->rsmem.heap[i].rs_heap_size;
            min_heap_num  = i;
        }
    }
    /* assign rightest 2M space for dtb */
    boot->dtb_load_addr = boot->rsmem.heap[min_heap_num].rs_heap_addr + min_heap_size - SIZE_OF_2M;
    boot->dtb_size      = DTB_SIZE;
    /* resize the heap */
    boot->rsmem.heap[min_heap_num].rs_heap_size -= SIZE_OF_2M;
#else
    boot->dtb_load_addr = DTB_A1000_ADDR;
    boot->dtb_size      = DTB_SIZE;
#endif
}

#endif /* __A1000_ADDR_H */