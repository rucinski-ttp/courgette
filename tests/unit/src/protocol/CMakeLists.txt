add_executable(test_protocol_encode_decode test_protocol_encode_decode.cpp)
target_link_libraries(test_protocol_encode_decode PRIVATE protocol util GTest::gtest_main)
target_include_directories(test_protocol_encode_decode PRIVATE ${ROOT_DIR}/lib/protocol/include ${ROOT_DIR}/lib/util/include)
include(GoogleTest)
gtest_discover_tests(test_protocol_encode_decode)

