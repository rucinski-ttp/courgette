#include "app/app_runtime.h"
#include <zephyr/kernel.h>

void app_run_forever(void)
{
    for (;;)
    {
        k_sleep(K_MSEC(1000));
    }
}
