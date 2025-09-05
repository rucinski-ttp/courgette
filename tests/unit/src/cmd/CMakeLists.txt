add_executable(test_cmd_dispatch test_cmd_dispatch.cpp)
target_link_libraries(test_cmd_dispatch PRIVATE cmd_dispatch cmd_echo cmd_version util GTest::gtest_main)
target_include_directories(test_cmd_dispatch PRIVATE ${ROOT_DIR}/lib/cmd/include ${ROOT_DIR}/app)
include(GoogleTest)
gtest_discover_tests(test_cmd_dispatch)

