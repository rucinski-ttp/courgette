#include "app/doom_task.h"

#include <stdatomic.h>
#include <zephyr/kernel.h>

/* Minimal scaffold for Doom: a 35 Hz loop that increments ticks. */

static K_THREAD_STACK_DEFINE(doom_stack, 4096);
static struct k_thread doom_thread_desc;
static atomic_int doom_state = ATOMIC_VAR_INIT(DOOM_STATE_STOPPED);
static atomic_uint doom_ticks = ATOMIC_VAR_INIT(0u);

static void doom_thread(void* a, void* b, void* c)
{
    ARG_UNUSED(a);
    ARG_UNUSED(b);
    ARG_UNUSED(c);
    atomic_store(&doom_state, DOOM_STATE_RUNNING);
    /* Target ~35 Hz (Doom ticrate) */
    const int period_ms = 1000 / 35;
    while (atomic_load(&doom_state) == DOOM_STATE_RUNNING)
    {
        /* TODO: hook doomgeneric frame render here */
        atomic_fetch_add(&doom_ticks, 1u);
        k_msleep(period_ms);
    }
    atomic_store(&doom_state, DOOM_STATE_STOPPED);
}

int doom_task_start(void)
{
    int st = atomic_load(&doom_state);
    if (st == DOOM_STATE_RUNNING || st == DOOM_STATE_STARTING)
    {
        return 0;
    }
    atomic_store(&doom_ticks, 0u);
    atomic_store(&doom_state, DOOM_STATE_STARTING);
    k_thread_create(&doom_thread_desc, doom_stack, K_THREAD_STACK_SIZEOF(doom_stack), doom_thread,
                    NULL, NULL, NULL, K_LOWEST_APPLICATION_THREAD_PRIO, 0, K_NO_WAIT);
    k_thread_name_set(&doom_thread_desc, "doom");
    return 0;
}

int doom_task_stop(void)
{
    int st = atomic_load(&doom_state);
    if (st == DOOM_STATE_RUNNING)
    {
        atomic_store(&doom_state, DOOM_STATE_STOPPED);
        return 0;
    }
    return st == DOOM_STATE_STOPPED ? 0 : -1;
}

void doom_task_status(doom_state_t* out_state, uint32_t* out_ticks)
{
    if (out_state)
    {
        *out_state = (doom_state_t)atomic_load(&doom_state);
    }
    if (out_ticks)
    {
        *out_ticks = (uint32_t)atomic_load(&doom_ticks);
    }
}
