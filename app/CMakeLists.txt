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
)
target_include_directories(util PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/util/include)
app_strict_warnings(util)

add_library(protocol STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/protocol/protocol.c
)
target_include_directories(protocol PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/../lib/protocol/include)
app_strict_warnings(protocol)

# --- Zephyr/board specific libraries ---
add_library(drivers STATIC
  src/drivers/heartbeat.c
)
target_include_directories(drivers PUBLIC ${APP_INCLUDE_DIR})
target_link_libraries(drivers PRIVATE zephyr_interface)
app_zephyr_warnings(drivers)

add_library(platform STATIC
  src/platform/serial_stub.c
)
target_include_directories(platform PUBLIC ${APP_INCLUDE_DIR})
target_link_libraries(platform PRIVATE zephyr_interface)
app_zephyr_warnings(platform)

# --- Application main ---
target_sources(app PRIVATE
  src/app/main.c
)

target_link_libraries(app PRIVATE drivers platform protocol util)
