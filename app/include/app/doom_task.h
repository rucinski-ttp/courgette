#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum
    {
        DOOM_STATE_STOPPED = 0,
        DOOM_STATE_STARTING = 1,
        DOOM_STATE_RUNNING = 2,
    } doom_state_t;

    /* Start the Doom thread if not running. Returns 0 on success, <0 on error. */
    int doom_task_start(void);

    /* Stop the Doom thread (best-effort, may not be immediate). */
    int doom_task_stop(void);

    /* Query state and tick counter (monotonic loop iterations). */
    void doom_task_status(doom_state_t* out_state, uint32_t* out_ticks);

#ifdef __cplusplus
}
#endif
