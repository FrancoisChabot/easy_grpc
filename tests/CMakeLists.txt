find_package(Protobuf REQUIRED)

include(GoogleTest)
find_package(GTest REQUIRED CONFIG)

set(GENERATED_PROTOBUF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})

add_custom_command(
                OUTPUT  "${GENERATED_PROTOBUF_PATH}/test.egrpc.pb.h"
                        "${GENERATED_PROTOBUF_PATH}/test.egrpc.pb.cc"
                COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                ARGS 
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--sgrpc_out=${GENERATED_PROTOBUF_PATH}"
                "--plugin=protoc-gen-sgrpc=$<TARGET_FILE:easy_grpc_protoc_plugin>"
                "${CMAKE_CURRENT_SOURCE_DIR}/test.proto"
                MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/test.proto
                DEPENDS easy_grpc_protoc_plugin
          )

add_custom_command(
                OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/generated/test.pb.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/generated/test.pb.cc"
                COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                ARGS 
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--cpp_out=${GENERATED_PROTOBUF_PATH}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test.proto"
                MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/test.proto
          )

add_executable(easy_grpc_tests 
  generated/test.egrpc.pb.cc
  generated/test.pb.cc
  binary_protocol.cpp
  end_to_end.cpp
  future.cpp
)

target_link_libraries(easy_grpc_tests easy_grpc GTest::gtest_main GTest::gtest GTest::gmock protobuf::libprotobuf)
target_include_directories(easy_grpc_tests PRIVATE .)

gtest_discover_tests(easy_grpc_tests)