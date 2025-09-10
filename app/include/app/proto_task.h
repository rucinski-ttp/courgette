#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

    /* Start protocol RX thread */
    void proto_task_start(void);
    /* Start periodic tick LOG thread */
    void proto_start_tick(void);

#ifdef __cplusplus
}
#endif
