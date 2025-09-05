cmake_minimum_required(VERSION 3.20)
project(unit_tests C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

include(FetchContent)

function(host_strict_warnings target)
  if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
  if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
  endif()
endfunction()

set(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../..")

# Portable library targets (mirror app linkage)
add_library(util STATIC ${ROOT_DIR}/lib/util/ring_buffer.c ${ROOT_DIR}/lib/util/crc32.c)
target_include_directories(util PUBLIC ${ROOT_DIR}/lib/util/include)
host_strict_warnings(util)

add_library(protocol STATIC ${ROOT_DIR}/lib/protocol/protocol.c)
target_include_directories(protocol PUBLIC ${ROOT_DIR}/lib/protocol/include ${ROOT_DIR}/lib/util/include)
host_strict_warnings(protocol)

add_library(cmd_dispatch STATIC ${ROOT_DIR}/lib/cmd/dispatch.c)
target_include_directories(cmd_dispatch PUBLIC ${ROOT_DIR}/lib/cmd/include)
host_strict_warnings(cmd_dispatch)

add_library(cmd_echo STATIC ${ROOT_DIR}/lib/cmd/echo.c)
target_include_directories(cmd_echo PUBLIC ${ROOT_DIR}/lib/cmd/include)
host_strict_warnings(cmd_echo)

add_library(cmd_version STATIC ${ROOT_DIR}/lib/cmd/version.c)
target_include_directories(cmd_version PUBLIC ${ROOT_DIR}/lib/cmd/include ${ROOT_DIR}/app)
host_strict_warnings(cmd_version)

# Inject git hash into cmd_version for host builds as well
execute_process(
  COMMAND git rev-parse --short=12 HEAD
  WORKING_DIRECTORY ${ROOT_DIR}
  OUTPUT_VARIABLE GIT_HASH
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT GIT_HASH)
  set(GIT_HASH "unknown")
endif()
target_compile_definitions(cmd_version PRIVATE APP_GIT_HASH="${GIT_HASH}")

# GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Each unit test lives in its own subfolder target
add_subdirectory(src/ring_buffer)
add_subdirectory(src/protocol)
add_subdirectory(src/cmd)
