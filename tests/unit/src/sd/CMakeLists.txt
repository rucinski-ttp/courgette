add_executable(test_sd_cmd
  test_sd_cmd.cpp
  ${ROOT_DIR}/app/src/app/cmd_sd.c
  fake_sd_ops.c
  stubs.c
)
target_include_directories(test_sd_cmd PRIVATE
  ${ROOT_DIR}/lib/cmd/include
  ${ROOT_DIR}/app/include
  ${ROOT_DIR}/lib/util/include
  ${ROOT_DIR}/lib/protocol/include
  ${ROOT_DIR}/app
)
target_compile_definitions(test_sd_cmd PRIVATE HOST_BUILD)
target_link_libraries(test_sd_cmd PRIVATE cmd_dispatch protocol util GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(test_sd_cmd)
