protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS data.proto)

set(GENERATED_PROTOBUF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/generated)

file(MAKE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/generated)

add_custom_command(
                OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/generated/data.egrpc.pb.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/generated/data.egrpc.pb.cc"
                COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                ARGS 
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--sgrpc_out=${GENERATED_PROTOBUF_PATH}"
                "--plugin=protoc-gen-sgrpc=$<TARGET_FILE:easy_grpc_protoc_plugin>"
                "${CMAKE_CURRENT_SOURCE_DIR}/data.proto"
                MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/data.proto
                DEPENDS easy_grpc_protoc_plugin
          )

add_custom_command(
      OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/generated/data.grpc.pb.h"
              "${CMAKE_CURRENT_SOURCE_DIR}/generated/data.grpc.pb.cc"
      COMMAND ${Protobuf_PROTOC_EXECUTABLE}
      ARGS 
      "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
      "--grpc_out=${GENERATED_PROTOBUF_PATH}"
      "--plugin=protoc-gen-grpc=/usr/local/bin/grpc_cpp_plugin"
      "${CMAKE_CURRENT_SOURCE_DIR}/data.proto"
      MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/data.proto
)

add_custom_command(
                OUTPUT  "${CMAKE_CURRENT_SOURCE_DIR}/generated/data.pb.h"
                        "${CMAKE_CURRENT_SOURCE_DIR}/generated/data.pb.cc"
                COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                ARGS 
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--cpp_out=${GENERATED_PROTOBUF_PATH}"
                "${CMAKE_CURRENT_SOURCE_DIR}/data.proto"
                MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/data.proto
          )


add_executable(simple_async_client 
  main.cpp
  generated/data.egrpc.pb.cc
  generated/data.pb.cc
)

add_executable(grpc_srv 
  main_server_grpc.cpp
  generated/data.grpc.pb.cc
  generated/data.pb.cc
)

target_link_libraries(grpc_srv
    -Wl,--whole-archive grpc++_reflection.a -Wl,--no-whole-archive
    grpc++.a
    grpc.a
    gpr.a
    protobuf::libprotobuf
    Threads::Threads
)


target_include_directories(
  simple_async_client 
PUBLIC 
  .
  ../../include)

target_link_libraries(simple_async_client easy_grpc)