#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Error bit definitions for visual LED code. */
    enum
    {
        ERR_UART_NOT_READY = 1u << 0, /* code 1 */
        ERR_SD_NOT_READY = 1u << 1,   /* code 2 */
    };

    /* Initialize the error indicator task (non-fatal if LED unavailable). */
    int error_indicator_init(void);

    /* Set/clear active error bits; thread will blink codes accordingly. */
    void error_indicator_set(uint32_t mask);
    void error_indicator_add(uint32_t bits);
    void error_indicator_clear(uint32_t bits);

    /* Query if any error is currently asserted. */
    bool error_indicator_active(void);

#ifdef __cplusplus
}
#endif
