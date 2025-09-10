#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    /* Application idle loop, kept out of main.c for clarity. */
    void app_run_forever(void);

#ifdef __cplusplus
}
#endif
