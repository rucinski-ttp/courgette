#pragma once

#include <zephyr/kernel.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Initialize and start the heartbeat LED thread. */
    int heartbeat_init(void);

#ifdef __cplusplus
}
#endif
