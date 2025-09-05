#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    int platform_serial_init(void);
    size_t platform_serial_available(void);
    size_t platform_serial_read(uint8_t* out, size_t max_len);
    int platform_serial_write(const uint8_t* data, size_t len);

#ifdef __cplusplus
}
#endif
