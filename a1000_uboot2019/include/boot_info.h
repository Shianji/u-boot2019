#ifndef __BOOT_INFO__
#define __BOOT_INFO__

#define BOOTINFO_MAX_HEAP_COUNT    (4)

#define ALIGN_128B __attribute__((aligned(128)))
typedef struct ALIGN_128B mem_distrib_t {
    u64 sram_pa_start;
    u64 sram_pa_size;
    u64 ddr0_pa_start;
    u64 ddr0_pa_size;
    u64 ddr1_pa_start;
    u64 ddr1_pa_size;
} mem_distrib;

typedef struct ALIGN_128B kmem_distrib_t {
    u64 kernel_img_addr;
    u32 kernel_img_maxsize; /* kernel maxsize < 4GB */
    u32 rsvd;               /* reserved, for 128b alignment */
} kmem_distrib;

typedef struct ALIGN_128B rs_heap_info_t {
    u64 rs_heap_addr;
    u64 rs_heap_size;
} rs_heap_info;

typedef struct ALIGN_128B rsmem_distrib_t {
    u64 rs_img_addr;      /* = kernel_img_addr + kernel_img_maxsize */
    u64 rs_stackres_addr;
    u32 rs_img_maxsize;   /* rs_img_maxsize < 4GB */
    u32 rs_stackres_size; /* rs_stackres_size < 4GB */
    u64 rsvd;             /* reserved, for 128b alignment */
    rs_heap_info heap[BOOTINFO_MAX_HEAP_COUNT];
} rsmem_distrib;

typedef struct ALIGN_128B boot_info_t
{
    u64 gic_base;
    u64 uart_base;
    u32 gic_size;       /* gic_size < 4GB */
    u32 uart_size;      /* uart_size < 4GB */
    u8 enable_hyper;    /* 0: rtos 1: hyper */
    u8 rsvd;            /* reserved, for 128b alignment */
    u16 rsvd1;          /* reserved, for 128b alignment */
    u32 rsvd2;          /* reserved, for 128b alignment */
    mem_distrib mem;
    kmem_distrib kmem;
    rsmem_distrib rsmem;
    u64 dtb_load_addr;
    u32 dtb_size;
    u32 rsvd3;
} boot_info;

#endif
