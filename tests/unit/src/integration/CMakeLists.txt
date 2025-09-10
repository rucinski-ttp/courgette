add_executable(test_proto_sd_roundtrip
  test_proto_sd_roundtrip.cpp
  ${ROOT_DIR}/app/src/app/cmd_sd.c
  ${ROOT_DIR}/lib/cmd/echo.c
  ${ROOT_DIR}/lib/cmd/version.c
  ${ROOT_DIR}/lib/cmd/mem.c
  ${ROOT_DIR}/tests/unit/src/sd/fake_sd_ops.c
  ${ROOT_DIR}/tests/unit/src/sd/stubs.c
)
target_include_directories(test_proto_sd_roundtrip PRIVATE
  ${ROOT_DIR}/lib/protocol/include
  ${ROOT_DIR}/lib/util/include
  ${ROOT_DIR}/lib/cmd/include
  ${ROOT_DIR}/app/include
  ${ROOT_DIR}/app
)
target_compile_definitions(test_proto_sd_roundtrip PRIVATE HOST_BUILD)
target_link_libraries(test_proto_sd_roundtrip PRIVATE protocol util cmd_dispatch GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(test_proto_sd_roundtrip)
