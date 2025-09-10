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
        CMD_ID_SD_FORMAT = 0x0200,
        CMD_ID_SD_LIST = 0x0201,
        CMD_ID_SD_READ = 0x0202,
        CMD_ID_SD_WRITE = 0x0203,
        CMD_ID_SD_RENAME = 0x0204,
        CMD_ID_SD_DELETE = 0x0205,
        CMD_ID_SD_MKDIR = 0x0206,
        CMD_ID_SD_STAT = 0x0207,
        CMD_ID_SD_CHECKSUM = 0x0208,
        CMD_ID_SD_STATUS = 0x0209,
        CMD_ID_SD_PEEK = 0x020A,
        CMD_ID_SD_RAWREAD = 0x020B,
        CMD_ID_SD_FILL = 0x020C,
    };

    typedef int (*cmd_handler_fn)(const uint8_t* req, uint32_t req_len, uint8_t* rsp,
                                  uint32_t* rsp_len);

    int cmd_register(uint16_t cmd_id, cmd_handler_fn fn);
    int cmd_dispatch(uint16_t cmd_id, const uint8_t* req, uint32_t req_len, uint8_t* rsp,
                     uint32_t* rsp_len);

#ifdef __cplusplus
}
#endif
