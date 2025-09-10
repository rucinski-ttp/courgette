#include <stddef.h>
#include <stdint.h>

int platform_serial_write(const uint8_t* data, size_t len)
{
    (void)data;
    (void)len;
    return 0;
}

void activity_led_pulse(void) {}
