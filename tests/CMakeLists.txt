include(GoogleTest)
find_package(GTest REQUIRED CONFIG)

add_executable(easy_grpc_tests 
  future.cpp
)

target_link_libraries(easy_grpc_tests easy_grpc GTest::gtest_main GTest::gtest GTest::gmock)
target_include_directories(easy_grpc_tests PRIVATE .)

gtest_discover_tests(easy_grpc_tests)