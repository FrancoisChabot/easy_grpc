find_package(Protobuf REQUIRED)

set(GENERATED_PROTOBUF_PATH ${CMAKE_CURRENT_SOURCE_DIR}/generated)
file(MAKE_DIRECTORY ${GENERATED_PROTOBUF_PATH})

add_custom_command(
                OUTPUT  "${GENERATED_PROTOBUF_PATH}/reflection.egrpc.pb.h"
                        "${GENERATED_PROTOBUF_PATH}/reflection.egrpc.pb.cc"
                COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                ARGS 
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--sgrpc_out=${GENERATED_PROTOBUF_PATH}"
                "--plugin=protoc-gen-sgrpc=$<TARGET_FILE:easy_grpc_protoc_plugin>"
                "${CMAKE_CURRENT_SOURCE_DIR}/reflection.proto"
                MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/reflection.proto
                DEPENDS easy_grpc_protoc_plugin
          )

add_custom_command(
                OUTPUT  "${GENERATED_PROTOBUF_PATH}/reflection.pb.h"
                        "${GENERATED_PROTOBUF_PATH}/reflection.pb.cc"
                COMMAND ${Protobuf_PROTOC_EXECUTABLE}
                ARGS 
                "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
                "--cpp_out=${GENERATED_PROTOBUF_PATH}"
                "${CMAKE_CURRENT_SOURCE_DIR}/reflection.proto"
                MAIN_DEPENDENCY ${CMAKE_CURRENT_SOURCE_DIR}/reflection.proto
          )

add_library(easy_grpc_reflection
  generated/reflection.egrpc.pb.cc
  generated/reflection.pb.cc
  reflection_impl.cpp
)

target_include_directories(easy_grpc_reflection
  PUBLIC ../include
  PRIVATE ..
)

target_link_libraries(easy_grpc_reflection easy_grpc)