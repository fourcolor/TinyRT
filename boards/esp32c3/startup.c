/*
 * startup.c -- ESP32-C3 Direct Boot startup
 *
 * Direct Boot sequence:
 *   ROM detects magic (0xaedb041d x2) at flash offset 0x0
 *   -> enables XIP cache
 *   -> jumps to flash offset 0x8 (VMA 0x42000008) = _entry
 *   -> _entry jumps to _start
 *   -> _start sets up gp/tp/sp/mscratch/mstatus, jumps to _cstart
 *   -> _cstart zeros .bss, installs mtvec, calls kernel_main()
 *
 * tp strategy: all tasks share the linker-reserved __tp_start scratch area.
 * Switch to per-task TLS blocks
 * when __thread variables or newlib reentrant I/O are needed.
 *
 * ISR stack strategy:
 *   mscratch always holds the ISR stack top when not in ISR.
 *   On trap entry _isr swaps sp <-> mscratch, so all ISR work runs on the
 *   dedicated isr_stack instead of the current task stack.
 *   mscratch is zeroed while inside the ISR so nested trap detection works:
 *   if mscratch == 0 at entry we are already on the ISR stack (nested trap).
 *
 * Interrupt enable order (RISC-V rule):
 *   1. mtvec  -- must be valid before any interrupt can fire
 *   2. mie    -- enable specific interrupt sources
 *   3. mstatus.MIE -- global enable (done in main() when ready)
 */

#include <stdint.h>
#include <stddef.h>
#include "intr.h"
#include "hal.h"
#include "task_private.h"
#include "kernel.h"
#include "sched_private.h"

extern char __bss_start[], __BSS_END__[];
extern char __tp_start[];

/* Trap frame layout (must match sw/lw order in _isr):
 * [0-29] GPRs: ra gp tp t0-t6 s0-s11 a0-a7 (30 regs, x0 and sp excluded)
 * [30] mcause
 * [31] mepc
 * [32] mstatus
 * Total: 33 words = 132 bytes, padded to 144 for 16-byte alignment.
 * Frame is stored on the *task's own stack*, not on the ISR stack. */
#define CONTEXT_SIZE 144

/* Dedicated ISR stack -- all trap/interrupt handling runs here.
 * Sized to hold one full frame plus reasonable call depth for do_trap. */
#define ISR_STACK_SIZE 4096
static char isr_stack[ISR_STACK_SIZE] __attribute__((aligned(16)));

/* Pointer to ISR stack top; loaded into mscratch at boot and after each mret.
 * Stored as a variable so the _isr asm can reload it with la+lw. */
void *const isr_stack_top = isr_stack + ISR_STACK_SIZE;

/* C-level trap handler -- provided by the RTOS kernel.
 * Receives (task_sp, mcause) where task_sp points to the trap frame on the task stack.
 * Returns the frame SP to restore from (same = no switch, different = switch task).
 * When current_task is NULL (during boot), task_sp points to the boot stack frame. */
uint32_t *do_trap(uint32_t *task_sp, uint32_t cause);

void _cstart(void);

/*
 * _isr -- machine-mode trap entry (M-mode only, ISR stack for C code).
 *
 * mscratch convention:
 * not in ISR: mscratch = isr_stack_top (task is running)
 * in ISR: mscratch = 0 (nested trap detection)
 *
 * Frame is allocated on *task's own stack*. ISR stack is only used
 * for running C code (do_trap, scheduler).
 * do_trap(task_sp, cause) returns the next task's frame SP to restore.
 */
__attribute__((naked, aligned(4))) void _isr(void)
{
    asm volatile(
        /* Allocate the persistent trap frame directly on the interrupted task stack. */
        "addi sp, sp, -%0 \n"

        /* Save all GPRs except x0 (zero) and x2 (sp) on task stack.
         * sp points to the new frame base. */
        "sw ra, 0*4(sp) \n"
        "sw gp, 1*4(sp) \n"
        "sw tp, 2*4(sp) \n"
        "sw t0, 3*4(sp) \n"
        "sw t1, 4*4(sp) \n"
        "sw t2, 5*4(sp) \n"
        "sw s0, 6*4(sp) \n"
        "sw s1, 7*4(sp) \n"
        "sw a0, 8*4(sp) \n"
        "sw a1, 9*4(sp) \n"
        "sw a2, 10*4(sp) \n"
        "sw a3, 11*4(sp) \n"
        "sw a4, 12*4(sp) \n"
        "sw a5, 13*4(sp) \n"
        "sw a6, 14*4(sp) \n"
        "sw a7, 15*4(sp) \n"
        "sw s2, 16*4(sp) \n"
        "sw s3, 17*4(sp) \n"
        "sw s4, 18*4(sp) \n"
        "sw s5, 19*4(sp) \n"
        "sw s6, 20*4(sp) \n"
        "sw s7, 21*4(sp) \n"
        "sw s8, 22*4(sp) \n"
        "sw s9, 23*4(sp) \n"
        "sw s10, 24*4(sp) \n"
        "sw s11, 25*4(sp) \n"
        "sw t3, 26*4(sp) \n"
        "sw t4, 27*4(sp) \n"
        "sw t5, 28*4(sp) \n"
        "sw t6, 29*4(sp) \n"

        /* Save CSRs on task stack */
        "csrr a0, mcause \n"
        "sw a0, 30*4(sp) \n"
        "csrr a0, mepc \n"
        "sw a0, 31*4(sp) \n"
        "csrr a0, mstatus \n"
        "sw a0, 32*4(sp) \n"

        /* a0 = task frame pointer for do_trap(); t0 = ISR stack top. */
        "mv a0, sp \n"
        "la t0, isr_stack_top \n"
        "lw t0, 0(t0) \n"

        /* Zero mscratch: marks "inside ISR" for nested trap detection */
        "csrw mscratch, zero \n"

        /* Switch to ISR stack for C code */
        "mv sp, t0 \n"

        /* Call do_trap(task_frame_sp, cause). */
        "csrr a1, mcause \n"
        "call do_trap \n"
        /* a0 = next task's frame sp */

        /* Switch to next task's stack */
        "mv sp, a0 \n"

        /* Restore CSRs and mscratch before restoring scratch GPRs. */
        "lw t0, 31*4(sp) \n"
        "csrw mepc, t0 \n"
        "lw t0, 32*4(sp) \n"
        "csrw mstatus, t0 \n"
        "la t0, isr_stack_top \n"
        "lw t0, 0(t0) \n"
        "csrw mscratch, t0 \n"

        /* Restore all GPRs from frame on next task's stack */
        "lw ra, 0*4(sp) \n"
        "lw gp, 1*4(sp) \n"
        "lw tp, 2*4(sp) \n"
        "lw t0, 3*4(sp) \n"
        "lw t1, 4*4(sp) \n"
        "lw t2, 5*4(sp) \n"
        "lw s0, 6*4(sp) \n"
        "lw s1, 7*4(sp) \n"
        "lw a0, 8*4(sp) \n"
        "lw a1, 9*4(sp) \n"
        "lw a2, 10*4(sp) \n"
        "lw a3, 11*4(sp) \n"
        "lw a4, 12*4(sp) \n"
        "lw a5, 13*4(sp) \n"
        "lw a6, 14*4(sp) \n"
        "lw a7, 15*4(sp) \n"
        "lw s2, 16*4(sp) \n"
        "lw s3, 17*4(sp) \n"
        "lw s4, 18*4(sp) \n"
        "lw s5, 19*4(sp) \n"
        "lw s6, 20*4(sp) \n"
        "lw s7, 21*4(sp) \n"
        "lw s8, 22*4(sp) \n"
        "lw s9, 23*4(sp) \n"
        "lw s10, 24*4(sp) \n"
        "lw s11, 25*4(sp) \n"
        "lw t3, 26*4(sp) \n"
        "lw t4, 27*4(sp) \n"
        "lw t5, 28*4(sp) \n"
        "lw t6, 29*4(sp) \n"

        /* Restore sp to original task sp (frame base + frame size) */
        "addi sp, sp, %0 \n"
        "mret \n"

        :
        : "i"(CONTEXT_SIZE)
        : "memory");
}

__attribute__((naked, noreturn)) void task_start_first(uint32_t *frame_sp)
{
    (void)frame_sp;

    asm volatile(
        /* Start the first task through the same mret restore contract used by
         * normal context switches. This keeps first-task and later-task startup
         * semantics identical. */
        "mv sp, a0 \n"
        "li t0, 1 \n"
        "la t1, scheduler \n"
        "sw t0, %1(t1) \n"

        "lw t0, 31*4(sp) \n"
        "csrw mepc, t0 \n"
        "lw t0, 32*4(sp) \n"
        "csrw mstatus, t0 \n"
        "la t0, isr_stack_top \n"
        "lw t0, 0(t0) \n"
        "csrw mscratch, t0 \n"

        "lw ra, 0*4(sp) \n"
        "lw gp, 1*4(sp) \n"
        "lw tp, 2*4(sp) \n"
        "lw t0, 3*4(sp) \n"
        "lw t1, 4*4(sp) \n"
        "lw t2, 5*4(sp) \n"
        "lw s0, 6*4(sp) \n"
        "lw s1, 7*4(sp) \n"
        "lw a0, 8*4(sp) \n"
        "lw a1, 9*4(sp) \n"
        "lw a2, 10*4(sp) \n"
        "lw a3, 11*4(sp) \n"
        "lw a4, 12*4(sp) \n"
        "lw a5, 13*4(sp) \n"
        "lw a6, 14*4(sp) \n"
        "lw a7, 15*4(sp) \n"
        "lw s2, 16*4(sp) \n"
        "lw s3, 17*4(sp) \n"
        "lw s4, 18*4(sp) \n"
        "lw s5, 19*4(sp) \n"
        "lw s6, 20*4(sp) \n"
        "lw s7, 21*4(sp) \n"
        "lw s8, 22*4(sp) \n"
        "lw s9, 23*4(sp) \n"
        "lw s10, 24*4(sp) \n"
        "lw s11, 25*4(sp) \n"
        "lw t3, 26*4(sp) \n"
        "lw t4, 27*4(sp) \n"
        "lw t5, 28*4(sp) \n"
        "lw t6, 29*4(sp) \n"

        "addi sp, sp, %0 \n"
        "mret \n"
        :
        : "i"(CONTEXT_SIZE), "i"(offsetof(scheduler_t, started))
        : "memory");
}

/*
 * _vector_table -- vectored mtvec base (MODE=1).
 * Cause 0 (exception) → slot 0 → j _isr
 * Cause N (interrupt N, 1..31) → slot N → j _isr
 * ESP32-C3 WARL-masks mtvec BASE more strictly than the 32-slot table size.
 * Keep the table 256-byte aligned so vectored interrupt slots do not land in
 * neighboring code after mtvec is written.
 */
__attribute__((naked, aligned(256))) static void _vector_table(void)
{
    asm volatile(".option push\n"
                 ".option norvc\n" /* force 32-bit J, each slot = 4 bytes */
                 ".rept 32\n"
                 "jal   x0, _isr\n"
                 ".endr\n"
                 ".option pop\n");
}

/* Direct Boot entry point -- linker places this at flash offset 0x8 */
__attribute__((naked, section(".text.entry"))) void _entry(void)
{
    __asm__("j _start");
}

/* Reset vector: set up gp, tp, sp, mscratch, mstatus, then enter C */
__attribute__((naked, noreturn)) void _start(void)
{
    __asm__(
        /* gp: .option norelax prevents this "la gp" from being rewritten
         * as a GP-relative load (which would be circular). */
        ".option push                   \n"
        ".option norelax                \n"
        "la   gp, __global_pointer$     \n"
        ".option pop                    \n"
        /* tp: linker-reserved scratch, kept outside heap */
        "la   tp, __tp_start            \n"
        "la   sp, __stack_top           \n"
        /* mscratch = ISR stack top (convention: not-in-ISR state) */
        "la   t0, isr_stack_top         \n"
        "lw   t0, 0(t0)                 \n"
        "csrw mscratch, t0              \n"
        /* mstatus: MIE=0 (interrupts off), MPP=11 (machine mode) */
        "li   t0, 0x1800                \n"
        "csrw mstatus, t0               \n"
        "j    _cstart                   \n");
}

/* C-level init: zero .bss -> install mtvec -> enter kernel */
__attribute__((noreturn)) void _cstart(void)
{
    for (char *p = __bss_start; p < __BSS_END__; p++)
        *p = 0;

    wdt_disable();

    /* Install vectored trap table before any interrupt can fire.
     * MODE=1: exceptions → slot 0, interrupt N → slot N, all route to _isr.
     * The kernel enables global interrupts only when it is ready. */
    mtvec_set(_vector_table);

    kernel_main();
}
