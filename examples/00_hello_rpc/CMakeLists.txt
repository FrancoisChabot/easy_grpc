find_package(Protobuf REQUIRED)

set(GENERATED_PROTOBUF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/generated)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/generated)

add_custom_command(
                OUTPUT  "${GENERATED_PROTOBUF_PATH}/hello.egrpc.pb.h"
                        "${GENERATED_PROTOBUF_PATH}/hello.egrpc.pb.cc"
                COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                ARGS 
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--sgrpc_out=${GENERATED_PROTOBUF_PATH}"
                "--plugin=protoc-gen-sgrpc=$<TARGET_FILE:easy_grpc_protoc_plugin>"
                "${CMAKE_CURRENT_SOURCE_DIR}/hello.proto"
                MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/hello.proto
                DEPENDS easy_grpc_protoc_plugin
          )

add_custom_command(
                OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/generated/hello.pb.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/generated/hello.pb.cc"
                COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                ARGS 
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--cpp_out=${GENERATED_PROTOBUF_PATH}"
                "${CMAKE_CURRENT_SOURCE_DIR}/hello.proto"
                MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/hello.proto
          )

add_library(hello_proto
  generated/hello.egrpc.pb.cc
  generated/hello.pb.cc
)

target_include_directories(hello_proto PUBLIC .)
target_link_libraries(hello_proto easy_grpc protobuf::libprotobuf)

add_executable(hello_server 
  server_main.cpp
)

add_executable(hello_client 
  client_main.cpp
)

target_link_libraries(hello_server hello_proto)
target_link_libraries(hello_client hello_proto)