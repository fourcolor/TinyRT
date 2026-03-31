#include "app_tests.h"
#include "board.h"
#include "csr.h"
#include "hal.h"
#include "logger.h"

void app_main(void)
{
    gpio_output(BOARD_LED_PIN);

    LOG_DEBUG("MVENDORID: %lu\n", read_csr(CSR_MVENDORID));
    LOG_DEBUG("MARCHID: %lu\n", read_csr(CSR_MARCHID));
    LOG_DEBUG("MIMPID: %lu\n", read_csr(CSR_MIMPID));
    LOG_DEBUG("MHARTID: %lu\n", read_csr(CSR_MHARTID));
    LOG_DEBUG("MSTATUS: %lu\n", read_csr(CSR_MSTATUS));
    LOG_DEBUG("MISA: %lu\n", read_csr(CSR_MISA));
    LOG_DEBUG("MTVEC: %lu\n", read_csr(CSR_MTVEC));
    LOG_DEBUG("MSCRATCH: %lu\n", read_csr(CSR_MSCRATCH));
    LOG_DEBUG("MEPC: %lu\n", read_csr(CSR_MEPC));
    LOG_DEBUG("MCAUSE: %lu\n", read_csr(CSR_MCAUSE));
    LOG_DEBUG("MTVAL: %lu\n", read_csr(CSR_MTVAL));

    app_test_timer_start();
    app_test_sync_start();
    app_test_isr_start();
}
