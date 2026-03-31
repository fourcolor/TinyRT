#pragma once
#include <stdint.h>

/* Context frame layout (word indices):
 *  [0-11]  s0-s11  callee-saved GPRs
 *  [12]    gp      global pointer
 *  [13]    tp      thread pointer
 *  [14]    sp      stack pointer
 *  [15]    ra      return address (= resume PC after longjmp)
 *  [16]    mstatus machine status CSR
 */
typedef uint32_t jmp_buf[17];

/* setjmp must not be inlined: saved ra/sp must reflect the call site. */
__attribute__((noinline)) static int setjmp(jmp_buf env)
{
    asm volatile("sw  s0,   0*4(%0) \n"
                 "sw  s1,   1*4(%0) \n"
                 "sw  s2,   2*4(%0) \n"
                 "sw  s3,   3*4(%0) \n"
                 "sw  s4,   4*4(%0) \n"
                 "sw  s5,   5*4(%0) \n"
                 "sw  s6,   6*4(%0) \n"
                 "sw  s7,   7*4(%0) \n"
                 "sw  s8,   8*4(%0) \n"
                 "sw  s9,   9*4(%0) \n"
                 "sw  s10, 10*4(%0) \n"
                 "sw  s11, 11*4(%0) \n"
                 "sw  gp,  12*4(%0) \n"
                 "sw  tp,  13*4(%0) \n"
                 "sw  sp,  14*4(%0) \n"
                 "sw  ra,  15*4(%0) \n"
                 "csrr t0, mstatus  \n"
                 "sw   t0, 16*4(%0) \n"
                 "li   a0, 0        \n"
                 :
                 : "r"(env)
                 : "t0", "memory", "a0");
    return 0;
}

__attribute__((noreturn)) static void longjmp(jmp_buf env, int val)
{
    if (val == 0)
        val = 1;

    asm volatile("lw  t0, 16*4(%0)  \n"
                 "csrw mstatus, t0  \n"
                 "lw  s0,   0*4(%0) \n"
                 "lw  s1,   1*4(%0) \n"
                 "lw  s2,   2*4(%0) \n"
                 "lw  s3,   3*4(%0) \n"
                 "lw  s4,   4*4(%0) \n"
                 "lw  s5,   5*4(%0) \n"
                 "lw  s6,   6*4(%0) \n"
                 "lw  s7,   7*4(%0) \n"
                 "lw  s8,   8*4(%0) \n"
                 "lw  s9,   9*4(%0) \n"
                 "lw  s10, 10*4(%0) \n"
                 "lw  s11, 11*4(%0) \n"
                 "lw  gp,  12*4(%0) \n"
                 "lw  tp,  13*4(%0) \n"
                 "lw  sp,  14*4(%0) \n"
                 "lw  ra,  15*4(%0) \n"
                 "mv  a0,  %1       \n"
                 "ret               \n"
                 :
                 : "r"(env), "r"(val)
                 : "memory");
    __builtin_unreachable();
}
