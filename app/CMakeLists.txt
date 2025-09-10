cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(courgette_bringup C)

# --- Include paths ---
set(APP_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories(${APP_INCLUDE_DIR})

# --- Helper: warnings for our targets only ---
function(app_strict_warnings target)
  if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
      -std=c11
      -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow
      -Wdouble-promotion -Wfloat-equal -Wformat=2 -Wundef -Werror)
  endif()
endfunction()

function(app_zephyr_warnings target)
  if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE
      -std=c11
      -Wall -Wextra -Wformat=2 -Wshadow -Werror
      # Avoid flags that commonly trip Zephyr headers
      -Wno-sign-conversion -Wno-conversion -Wno-pedantic -Wno-float-equal
      -Wno-double-promotion)
  endif()
endfunction()

# --- Portable libraries (shareable with host tests) ---
add_library(util STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/util/ring_buffer.c
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/util/crc32.c
)
target_include_directories(util PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/util/include)
target_link_libraries(util PRIVATE zephyr_interface)
app_strict_warnings(util)

add_library(protocol STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/protocol/protocol.c
)
target_include_directories(protocol PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/protocol/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/util/include)
target_link_libraries(protocol PRIVATE zephyr_interface)
app_strict_warnings(protocol)

# --- Zephyr/board specific libraries ---
add_library(drivers STATIC
  src/drivers/heartbeat.c
  src/drivers/error_indicator.c
)
target_include_directories(drivers PUBLIC ${APP_INCLUDE_DIR})
target_link_libraries(drivers PRIVATE zephyr_interface)
app_zephyr_warnings(drivers)

add_library(platform STATIC
  src/platform/serial_async.c
  src/platform/fatal.c
)
target_include_directories(platform PUBLIC ${APP_INCLUDE_DIR})
target_include_directories(platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/util/include)
target_include_directories(platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/protocol/include)
# Access FatFS public headers (ff.h) for FATFS fs_data type
target_include_directories(platform PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../modules/fs/fatfs/include)
target_include_directories(platform PRIVATE ${ZEPHYR_BASE}/modules/fatfs)
target_link_libraries(platform PRIVATE zephyr_interface)
app_zephyr_warnings(platform)

# --- Application main ---
target_sources(app PRIVATE
  src/app/main.c
  src/app/app_runtime.c
  src/app/proto_task.c
  src/app/cmd_sd.c
  src/drivers/activity_led.c
  src/platform/sd_ops.c
)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../modules/fs/fatfs/include)
target_include_directories(app PRIVATE ${ZEPHYR_BASE}/modules/fatfs)
target_compile_definitions(app PRIVATE ZEPHYR_CONFIG_OVERRIDE=zephyr_fatfs_config.h)

add_library(cmd_dispatch STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/dispatch.c)
target_include_directories(cmd_dispatch PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/include)
target_link_libraries(cmd_dispatch PRIVATE zephyr_interface)
app_strict_warnings(cmd_dispatch)

add_library(cmd_echo STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/echo.c)
target_include_directories(cmd_echo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/include)
target_link_libraries(cmd_echo PRIVATE zephyr_interface)
app_strict_warnings(cmd_echo)

add_library(cmd_version STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/version.c)
target_include_directories(cmd_version PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/include)
target_link_libraries(cmd_version PRIVATE zephyr_interface)
app_strict_warnings(cmd_version)

add_library(cmd_reboot STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/reboot.c)
target_include_directories(cmd_reboot PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/include)
target_link_libraries(cmd_reboot PRIVATE zephyr_interface)
app_zephyr_warnings(cmd_reboot)

add_library(cmd_mem STATIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/mem.c)
target_include_directories(cmd_mem PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../lib/cmd/include)
target_link_libraries(cmd_mem PRIVATE zephyr_interface)
app_strict_warnings(cmd_mem)

target_link_libraries(app PRIVATE drivers platform protocol util cmd_echo cmd_version cmd_reboot cmd_dispatch cmd_mem)

# Embed git hash into version command via compile definition
execute_process(
  COMMAND git rev-parse --short=12 HEAD
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE)
set(GIT_DIRTY_SUFFIX "")
execute_process(
  COMMAND git diff-index --quiet HEAD --
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  RESULT_VARIABLE GIT_DIFF_RESULT
)
if(GIT_DIFF_RESULT)
  set(GIT_DIRTY_SUFFIX "-dirty")
endif()
if(NOT GIT_HASH)
  set(GIT_HASH "unknown")
endif()
target_compile_definitions(cmd_version PRIVATE APP_GIT_HASH="${GIT_HASH}${GIT_DIRTY_SUFFIX}")
