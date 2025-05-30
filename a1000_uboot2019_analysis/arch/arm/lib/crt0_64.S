/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * crt0 - C-runtime startup Code for AArch64 U-Boot
 *
 * (C) Copyright 2013
 * David Feng <fenghua@phytium.com.cn>
 *
 * (C) Copyright 2012
 * Albert ARIBAUD <albert.u.boot@aribaud.net>
 */

#include <config.h>
#include <asm-offsets.h>
#include <asm/macro.h>
#include <linux/linkage.h>

/*
 * This file handles the target-independent stages of the U-Boot
 * start-up where a C runtime environment is needed. Its entry point
 * is _main and is branched into from the target's start.S file.
 *
 * _main execution sequence is:
 *
 * 1. Set up initial environment for calling board_init_f().
 *    This environment only provides a stack and a place to store
 *    the GD ('global data') structure, both located in some readily
 *    available RAM (SRAM, locked cache...). In this context, VARIABLE
 *    global data, initialized or not (BSS), are UNAVAILABLE; only
 *    CONSTANT initialized data are available. GD should be zeroed
 *    before board_init_f() is called.
 *
 * 2. Call board_init_f(). This function prepares the hardware for
 *    execution from system RAM (DRAM, DDR...) As system RAM may not
 *    be available yet, , board_init_f() must use the current GD to
 *    store any data which must be passed on to later stages. These
 *    data include the relocation destination, the future stack, and
 *    the future GD location.
 *
 * 3. Set up intermediate environment where the stack and GD are the
 *    ones allocated by board_init_f() in system RAM, but BSS and
 *    initialized non-const data are still not available.
 *
 * 4a.For U-Boot proper (not SPL), call relocate_code(). This function
 *    relocates U-Boot from its current location into the relocation
 *    destination computed by board_init_f().
 *
 * 4b.For SPL, board_init_f() just returns (to crt0). There is no
 *    code relocation in SPL.
 *
 * 5. Set up final environment for calling board_init_r(). This
 *    environment has BSS (initialized to 0), initialized non-const
 *    data (initialized to their intended value), and stack in system
 *    RAM (for SPL moving the stack and GD into RAM is optional - see
 *    CONFIG_SPL_STACK_R). GD has retained values set by board_init_f().
 *
 * TODO: For SPL, implement stack relocation on AArch64.
 *
 * 6. For U-Boot proper (not SPL), some CPUs have some work left to do
 *    at this point regarding memory, so call c_runtime_cpu_setup.
 *
 * 7. Branch to board_init_r().
 *
 * For more information see 'Board Initialisation Flow in README.
 */

ENTRY(_main)

/*
 * Set up initial C runtime environment and call board_init_f(0).
 */
#if defined(CONFIG_TPL_BUILD) && defined(CONFIG_TPL_NEEDS_SEPARATE_STACK)
	ldr	x0, =(CONFIG_TPL_STACK)
#elif defined(CONFIG_SPL_BUILD) && defined(CONFIG_SPL_STACK)
	ldr	x0, =(CONFIG_SPL_STACK)
#elif defined(CONFIG_SYS_INIT_SP_BSS_OFFSET)
	adr	x0, __bss_start
	add	x0, x0, #CONFIG_SYS_INIT_SP_BSS_OFFSET
#else
#if defined(CONFIG_SPL_BUILD) && defined(CONFIG_TARGET_BST_A1000)
	ldr x0, _TEXT_SPL_BASE
	mov x1, DEBUG_SYS_SPL_MAGIC
	cmp x0, x1
	b.eq spl_sp_fix
	ldr	x0, =(CONFIG_SYS_INIT_SP_ADDR)
	b spl_sp_fix_done
spl_sp_fix:
	ldr	x0, =(DEBUG_SYS_INIT_SP_ADDR) 
spl_sp_fix_done:
#else
	ldr	x0, =(CONFIG_SYS_INIT_SP_ADDR)
#endif/* 到这里已经将sp对应的地址值写入到x0*/
#endif
	bic	sp, x0, #0xf	/* 16-byte alignment for ABI compliance 16字节对齐*/
	mov	x0, sp
	bl	board_init_f_alloc_reserve	/* 这个函数用于完成struct global_data全局数据结构的内存分配，将其存放在栈中，返回增长后的栈指针*/
	mov	sp, x0
	/* set up gd here, outside any C code */
	mov	x18, x0
	bl	board_init_f_init_reserve/* 这个函数用于完成struct global_data全局数据结构的内存初始化*/

	mov	x0, #0
	bl	board_init_f

#if !defined(CONFIG_SPL_BUILD)
/*
 * Set up intermediate environment (new sp and gd) and call
 * relocate_code(addr_moni). Trick here is that we'll return
 * 'here' but relocated.
 *//*此时x18中存储的是全局数据gd的起始地址（未重定位之前的，但此处gd中的数据已经在board_init_f函数中被设置了）*/
	ldr	x0, [x18, #GD_START_ADDR_SP]	/*获取重定位后的栈sp地址 x0 <- gd->start_addr_sp */
	bic	sp, x0, #0xf	/* 16字节对齐 16-byte alignment for ABI compliance */
	ldr	x18, [x18, #GD_NEW_GD]		/*获取重定位后的全局数据gd的地址 x18 <- gd->new_gd */

	adr	lr, relocation_return
#if CONFIG_POSITION_INDEPENDENT
	/* Add in link-vs-runtime offset */
	adr	x0, _start		/* x0 <- Runtime value of _start */
	ldr	x9, _TEXT_BASE		/* x9 <- Linked value of _start */
	sub	x9, x9, x0		/* x9 <- Run-vs-link offset */
	add	lr, lr, x9
#endif
	/* Add in link-vs-relocation offset */
	/*gd->reloc_off中存储着重定位的u-boot到当前镜像所在位置的偏移量：
		gd->reloc_off = gd->relocaddr - (unsigned long)__image_copy_start，
		__image_copy_start存储在.text不会进行动态重定位*/
	ldr	x9, [x18, #GD_RELOC_OFF]	/* x9 <- gd->reloc_off */
	add	lr, lr, x9	/* 此时lr中存储的地址就是u-boot重定位之后relocation_return的地址 new return address after relocation */
	ldr	x0, [x18, #GD_RELOCADDR]	/* x0 <- gd->relocaddr */
	b	relocate_code

relocation_return:

/*
 * Set up final (full) environment
 */
	bl	c_runtime_cpu_setup		/* still call old routine */
#endif /* !CONFIG_SPL_BUILD */
#if defined(CONFIG_SPL_BUILD)
	bl	spl_relocate_stack_gd           /*根据配置在SPL阶段将全局数据gd和栈移动到一个新的位置 may return NULL */
	/* set up gd here, outside any C code, if new stack is returned */
	cmp	x0, #0
	csel	x18, x0, x18, ne		/*如果gd和栈被重定位，则将返回值即gd的新地址读取到x18*/
	/*
	 * Perform 'sp = (x0 != NULL) ? x0 : sp' while working
	 * around the constraint that conditional moves can not
	 * have 'sp' as an operand
	 */
	mov	x1, sp
	cmp	x0, #0
	csel	x0, x0, x1, ne
	mov	sp, x0
#endif

/*
 * Clear BSS section
 */
	ldr	x0, =__bss_start		/* this is auto-relocated! */
	ldr	x1, =__bss_end			/* this is auto-relocated! */
clear_loop:
	str	xzr, [x0], #8
	cmp	x0, x1
	b.lo	clear_loop

	/* call board_init_r(gd_t *id, ulong dest_addr) */
	mov	x0, x18				/* gd_t */
	ldr	x1, [x18, #GD_RELOCADDR]	/* dest_addr */
	b	board_init_r			/* PC relative jump */

	/* NOTREACHED - board_init_r() does not return */

ENDPROC(_main)
