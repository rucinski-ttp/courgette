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
add_library(util STATIC ${ROOT_DIR}/lib/util/ring_buffer.c)
target_include_directories(util PUBLIC ${ROOT_DIR}/lib/util/include)
host_strict_warnings(util)

# Also compile protocol library to produce compile_commands for clang-tidy
add_library(protocol STATIC ${ROOT_DIR}/lib/protocol/protocol.c)
target_include_directories(protocol PUBLIC ${ROOT_DIR}/lib/protocol/include)
host_strict_warnings(protocol)

# GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(test_ring_buffer
  ${CMAKE_CURRENT_LIST_DIR}/src/test_ring_buffer.cpp)
target_link_libraries(test_ring_buffer PRIVATE util GTest::gtest_main)
host_strict_warnings(test_ring_buffer)

include(GoogleTest)
gtest_discover_tests(test_ring_buffer)
