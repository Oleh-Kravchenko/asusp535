#include <linux/init.h>
#include <linux/linkage.h>

#include <asm/assembler.h>
#include <asm/asm-offsets.h>
#include <asm/errno.h>
#include <asm/thread_info.h>

@ Bad Abort numbers
@ -----------------
@
#define BAD_PREFETCH	0
#define BAD_DATA	1
#define BAD_ADDREXCPTN	2
#define BAD_IRQ		3
#define BAD_UNDEFINSTR	4

@
@ Most of the stack format comes from struct pt_regs, but with
@ the addition of 8 bytes for storing syscall args 5 and 6.
@ This _must_ remain a multiple of 8 for EABI.
@
#define S_OFF		8

/* 
 * The SWI code relies on the fact that R0 is at the bottom of the stack
 * (due to slow/fast restore user regs).
 */
#if S_R0 != 0
#error "Please fix"
#endif

	.macro	zero_fp
#ifdef CONFIG_FRAME_POINTER
	mov	fp, #0
#endif
	.endm

	.macro	alignment_trap, rtemp
#ifdef CONFIG_ALIGNMENT_TRAP
	ldr	\rtemp, .LCcralign
	ldr	\rtemp, [\rtemp]
	mcr	p15, 0, \rtemp, c1, c0
#endif
	.endm

#ifdef CONFIG_CPU_V7M
/*
 * ARMv7-M exception entry/exit macros.
 *
 * xPSR, ReturnAddress(), LR (R14), R12, R3, R2, R1, and R0 are
 * automatically saved on the current stack (32 words) before
 * switching to the exception stack (SP_main).
 *
 * If exception is taken while in user mode, SP_main is
 * empty. Otherwise, SP_main is aligned to 64 bit automatically
 * (CCR.STKALIGN set).
 *
 * Linux assumes that the interrupts are disabled when entering an
 * exception handler and it may BUG if this is not the case. Interrupts
 * are disabled during entry and reenabled in the exit macro.
 *
 * v7m_exception_fast_exit is used when returning from interrupts.
 *
 * v7m_exception_slow_exit is used when returning from SVC or PendSV.
 * When returning to kernel mode, we don't return from exception.
 */
	.macro	v7m_exception_entry
	@ determine the location of the registers saved by the core during
	@ exception entry. Depending on the mode the cpu was in when the
	@ exception happend that is either on the main or the process stack.
	@ Bit 2 of EXC_RETURN stored in the lr register specifies which stack
	@ was used.
	tst	lr, #0x4
	mrsne	r12, psp
	moveq	r12, sp

	@ we cannot rely on r0-r3 and r12 matching the value saved in the
	@ exception frame because of tail-chaining. So these have to be
	@ reloaded.
	ldmia	r12!, {r0-r3}

	@ Linux expects to have irqs off. Do it here before taking stack space
	cpsid	i

	sub	sp, #S_FRAME_SIZE-S_IP
	stmdb	sp!, {r0-r11}

	@ load saved r12, lr, return address and xPSR.
	@ r0-r7 are used for signals and never touched from now on. Clobbering
	@ r8-r12 is OK.
	mov	r9, r12
	ldmia	r9!, {r8, r10-r12}

	@ calculate the original stack pointer value.
	@ r9 currently points to the memory location just above the auto saved
	@ xPSR. If the FP extension is implemented and bit 4 of EXC_RETURN is 0
	@ then space was allocated for FP state. That is space for 18 32-bit
	@ values. (If FP extension is unimplemented, bit 4 is 1.)
	@ Additionally the cpu might automatically 8-byte align the stack. Bit 9
	@ of the saved xPSR specifies if stack aligning took place. In this case
	@ another 32-bit value is included in the stack.

	tst	lr, #0x10
	addeq	r9, r9, #576

	tst	r12, 0x100
	addne	r9, r9, #4

	@ store saved r12 using str to have a register to hold the base for stm
	str	r8, [sp, #S_IP]
	add	r8, sp, #S_SP
	@ store r13-r15, xPSR
	stmia	r8!, {r9-r12}
	@ store r0 once more and EXC_RETURN
	stmia	r8, {r0, lr}
	.endm

	.macro	v7m_exception_fast_exit
	@ registers r0-r3 and r12 are automatically restored on exception
	@ return. r4-r7 were not clobbered in v7m_exception_entry so for
	@ correctness they don't need to be restored. So only r8-r11 must be
	@ restored here. The easiest way to do so is to restore r0-r7, too.
	ldmia	sp!, {r0-r11}
	add	sp, #S_FRAME_SIZE-S_IP
	cpsie	i
	bx	lr
	.endm

	.macro	v7m_exception_slow_exit ret_r0
	cpsid	i
	ldr	lr, [sp, #S_EXC_RET]	@ read exception LR
	tst     lr, #0x8
	bne	1f			@ go to thread mode using exception return

	/*
	 * return to kernel thread
	 * sp is already set up (and might be unset in pt_regs), so only
	 * restore r0-r12 and pc
	 */
	ldmia	sp, {r0-r12}
	ldr	lr, [sp, #S_PC]
	add	sp, sp, #S_FRAME_SIZE
	cpsie	i
	bx	lr

1:	/*
	 * return to userspace
	 */

	@ read original r12, sp, lr, pc and xPSR
	add	r12, sp, #S_IP
	ldmia	r12, {r1-r5}

	@ handle stack aligning
	tst	r5, #0x100
	subne	r2, r2, #4

	@ skip over stack space for fp saving
	tst	lr, #0x10
	subeq	r2, r2, #576

	@ write basic exception frame
	stmdb	r2!, {r1, r3-r5}
	ldmia	sp, {r1, r3-r5}
	.if	\ret_r0
	stmdb	r2!, {r0, r3-r5}
	.else
	stmdb	r2!, {r1, r3-r5}
	.endif

	@ restore process sp
	msr	psp, r2

	@ restore original r4-r11
	ldmia	sp!, {r0-r11}

	@ restore main sp
	add	sp, sp, #S_FRAME_SIZE-S_IP

	cpsie	i
	bx	lr
	.endm
#endif	/* CONFIG_CPU_V7M */

	@
	@ Store/load the USER SP and LR registers by switching to the SYS
	@ mode. Useful in Thumb-2 mode where "stm/ldm rd, {sp, lr}^" is not
	@ available. Should only be called from SVC mode
	@
	.macro	store_user_sp_lr, rd, rtemp, offset = 0
	mrs	\rtemp, cpsr
	eor	\rtemp, \rtemp, #(SVC_MODE ^ SYSTEM_MODE)
	msr	cpsr_c, \rtemp			@ switch to the SYS mode

	str	sp, [\rd, #\offset]		@ save sp_usr
	str	lr, [\rd, #\offset + 4]		@ save lr_usr

	eor	\rtemp, \rtemp, #(SVC_MODE ^ SYSTEM_MODE)
	msr	cpsr_c, \rtemp			@ switch back to the SVC mode
	.endm

	.macro	load_user_sp_lr, rd, rtemp, offset = 0
	mrs	\rtemp, cpsr
	eor	\rtemp, \rtemp, #(SVC_MODE ^ SYSTEM_MODE)
	msr	cpsr_c, \rtemp			@ switch to the SYS mode

	ldr	sp, [\rd, #\offset]		@ load sp_usr
	ldr	lr, [\rd, #\offset + 4]		@ load lr_usr

	eor	\rtemp, \rtemp, #(SVC_MODE ^ SYSTEM_MODE)
	msr	cpsr_c, \rtemp			@ switch back to the SVC mode
	.endm

#ifndef CONFIG_THUMB2_KERNEL
	.macro	svc_exit, rpsr
	msr	spsr_cxsf, \rpsr
#if defined(CONFIG_CPU_V6)
	ldr	r0, [sp]
	strex	r1, r2, [sp]			@ clear the exclusive monitor
	ldmib	sp, {r1 - pc}^			@ load r1 - pc, cpsr
#elif defined(CONFIG_CPU_32v6K)
	clrex					@ clear the exclusive monitor
	ldmia	sp, {r0 - pc}^			@ load r0 - pc, cpsr
#else
	ldmia	sp, {r0 - pc}^			@ load r0 - pc, cpsr
#endif
	.endm

	.macro	restore_user_regs, fast = 0, offset = 0
	ldr	r1, [sp, #\offset + S_PSR]	@ get calling cpsr
	ldr	lr, [sp, #\offset + S_PC]!	@ get pc
	msr	spsr_cxsf, r1			@ save in spsr_svc
#if defined(CONFIG_CPU_V6)
	strex	r1, r2, [sp]			@ clear the exclusive monitor
#elif defined(CONFIG_CPU_32v6K)
	clrex					@ clear the exclusive monitor
#endif
	.if	\fast
	ldmdb	sp, {r1 - lr}^			@ get calling r1 - lr
	.else
	ldmdb	sp, {r0 - lr}^			@ get calling r0 - lr
	.endif
	mov	r0, r0				@ ARMv5T and earlier require a nop
						@ after ldm {}^
	add	sp, sp, #S_FRAME_SIZE - S_PC
	movs	pc, lr				@ return & move spsr_svc into cpsr
	.endm

	.macro	get_thread_info, rd
	mov	\rd, sp, lsr #13
	mov	\rd, \rd, lsl #13
	.endm

	@
	@ 32-bit wide "mov pc, reg"
	@
	.macro	movw_pc, reg
	mov	pc, \reg
	.endm
#else	/* CONFIG_THUMB2_KERNEL */
	.macro	svc_exit, rpsr
	ldr	lr, [sp, #S_SP]			@ top of the stack
	ldrd	r0, r1, [sp, #S_LR]		@ calling lr and pc
	clrex					@ clear the exclusive monitor
	stmdb	lr!, {r0, r1, \rpsr}		@ calling lr and rfe context
	ldmia	sp, {r0 - r12}
	mov	sp, lr
	ldr	lr, [sp], #4
	rfeia	sp!
	.endm

#ifdef CONFIG_CPU_V7M
	.macro	restore_user_regs, fast = 0, offset = 0
	.if	\offset
	add	sp, #\offset
	.endif
	v7m_exception_slow_exit ret_r0 = \fast
	.endm
#else	/* !CONFIG_CPU_V7M */
	.macro	restore_user_regs, fast = 0, offset = 0
	clrex					@ clear the exclusive monitor
	mov	r2, sp
	load_user_sp_lr r2, r3, \offset + S_SP	@ calling sp, lr
	ldr	r1, [sp, #\offset + S_PSR]	@ get calling cpsr
	ldr	lr, [sp, #\offset + S_PC]	@ get pc
	add	sp, sp, #\offset + S_SP
	msr	spsr_cxsf, r1			@ save in spsr_svc
	.if	\fast
	ldmdb	sp, {r1 - r12}			@ get calling r1 - r12
	.else
	ldmdb	sp, {r0 - r12}			@ get calling r0 - r12
	.endif
	add	sp, sp, #S_FRAME_SIZE - S_SP
	movs	pc, lr				@ return & move spsr_svc into cpsr
	.endm
#endif	/* CONFIG_CPU_V7M */

	.macro	get_thread_info, rd
	mov	\rd, sp
	lsr	\rd, \rd, #13
	mov	\rd, \rd, lsl #13
	.endm

	@
	@ 32-bit wide "mov pc, reg"
	@
	.macro	movw_pc, reg
	mov	pc, \reg
	nop
	.endm
#endif	/* !CONFIG_THUMB2_KERNEL */

/*
 * These are the registers used in the syscall handler, and allow us to
 * have in theory up to 7 arguments to a function - r0 to r6.
 *
 * r7 is reserved for the system call number for thumb mode.
 *
 * Note that tbl == why is intentional.
 *
 * We must set at least "tsk" and "why" when calling ret_with_reschedule.
 */
scno	.req	r7		@ syscall number
tbl	.req	r8		@ syscall table pointer
why	.req	r8		@ Linux syscall (!= 0)
tsk	.req	r9		@ current thread_info
