#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    /* Shared command IDs (mirrors protocol header for convenience) */
    enum
    {
        CMD_ID_ECHO = 0x0001,
        CMD_ID_VERSION = 0x0002,
        CMD_ID_REBOOT = 0x0003,
        CMD_ID_LOG = 0x00FE,
        CMD_ID_MEM_READ = 0x0100,
        CMD_ID_MEM_WRITE = 0x0101,
    };

    typedef int (*cmd_handler_fn)(const uint8_t* req, uint32_t req_len, uint8_t* rsp,
                                  uint32_t* rsp_len);

    int cmd_register(uint16_t cmd_id, cmd_handler_fn fn);
    int cmd_dispatch(uint16_t cmd_id, const uint8_t* req, uint32_t req_len, uint8_t* rsp,
                     uint32_t* rsp_len);

#ifdef __cplusplus
}
#endif
