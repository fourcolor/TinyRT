/* Machine Information CSRs */

/* Machine Vendor ID (Read-only)*/
#define CSR_MVENDORID 0xF11
/* Machine Architecture ID (Read-only)*/
#define CSR_MARCHID 0xF12
/* Machine Implementation ID (Read-only)*/
#define CSR_MIMPID 0xF13
/* Machine Hart ID (Read-only)*/
#define CSR_MHARTID 0xF14

/* Machine Trap Setup CSRs */

/* Machine Mode Status */
#define CSR_MSTATUS 0x300
/* Machine ISA */
#define CSR_MISA 0x301
/* Machine Trap Vector */
#define CSR_MTVEC 0x305

/* Machine Trap Handling CSRs */

/* Machine Scratch  */
#define CSR_MSCRATCH 0x340
/* Machine Trap Program Counter */
#define CSR_MEPC 0x341
/* Machine Trap Cause */
#define CSR_MCAUSE 0x342
/* Machine Trap Value */
#define CSR_MTVAL 0x343

/* Physical Memory Protection (PMP) CSRs */

/* Physical memory protection configuration */
#define CSR_PMPCFG0 0x3A0
#define CSR_PMPCFG1 0x3A1
#define CSR_PMPCFG2 0x3A2
#define CSR_PMPCFG3 0x3A3
/* Physical memory protection address register */
#define CSR_PMPADDR0 0x3B0
#define CSR_PMPADDR1 0x3B1
#define CSR_PMPADDR2 0x3B2
#define CSR_PMPADDR3 0x3B3
#define CSR_PMPADDR4 0x3B4
#define CSR_PMPADDR5 0x3B5
#define CSR_PMPADDR6 0x3B6
#define CSR_PMPADDR7 0x3B7
#define CSR_PMPADDR8 0x3B8
#define CSR_PMPADDR9 0x3B9
#define CSR_PMPADDR10 0x3BB
#define CSR_PMPADDR12 0x3BC
#define CSR_PMPADDR13 0x3BD
#define CSR_PMPADDR14 0x3BE
#define CSR_PMPADDR15 0x3BF

/* Trigger Module CSRs (shared with Debug Mode) */

/* Trigger Select Register */
#define CSR_TSELECT 0x7A0
/* Trigger Abstract Data 1 */
#define CSR_TDATA1 0x7A1
/* Trigger Abstract Data 2 */
#define CSR_TDATA2 0x7A2
/* Global Trigger Control */
#define CSR_TCONTROL 0x7A5

/* Debug Mode CSRs */

/* Debug Control and Status */
#define CSR_DCSR 0x7B0
/* Debug PC */
#define CSR_DPC 0x7B1
/* Debug Scratch Register 0 */
#define CSR_DSCRATCH0 0x7B2
/* Debug Scratch Register 1 */
#define CSR_DSCRATCH1 0x7B3

/* Performance Counter CSRs (Custom) */

/* Machine Performance Counter Event */
#define CSR_MPCER 0x7E0
/* Machine Performance Counter Mode */
#define CSR_MPCMR 0x7E1
/* Machine Performance Counter Count */
#define CSR_MPCCR 0x7E2

/* GPIO Access CSRs (Custom) */

/* GPIO Output Enable */
#define CSR_CPU_GPIO_ENABLE 0x803
/* GPIO Input Value */
#define CSR_CPU_GPIO_IN 0x804
/* GPIO Output Value */
#define CSR_CPU_GPIO_OUT 0x805

#define read_csr(reg)                                                                              \
    ({                                                                                             \
        unsigned long __v;                                                                         \
        asm volatile("csrr %0, %1" : "=r"(__v) : "i"(reg));                                        \
        __v;                                                                                       \
    })

#define write_csr(reg, val) ({ asm volatile("csrw %0, %1" ::"i"(reg), "rK"(val)); })

#define set_csr(reg, val)                                                                          \
    ({                                                                                             \
        unsigned long __v;                                                                         \
        asm volatile("csrrs %0, %1, %2" : "=r"(__v) : "i"(reg), "rK"(val));                        \
        __v;                                                                                       \
    })

#define clear_csr(reg, val)                                                                        \
    ({                                                                                             \
        unsigned long __v;                                                                         \
        asm volatile("csrrc %0, %1, %2" : "=r"(__v) : "i"(reg), "rK"(val));                        \
        __v;                                                                                       \
    })
