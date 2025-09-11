add_executable(test_display_pixel_conv test_display_pixel_conv.cpp)
target_link_libraries(test_display_pixel_conv PRIVATE util GTest::gtest_main)
target_include_directories(test_display_pixel_conv PRIVATE ${ROOT_DIR}/lib/display/include)
include(GoogleTest)
gtest_discover_tests(test_display_pixel_conv)

