cmake_minimum_required(VERSION 3.10)

project(easy_grpc VERSION 0.0.1)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

add_subdirectory(protoc_plugin)

add_library(easy_grpc
  src/easy_grpc/client/channel.cpp
  src/easy_grpc/client/unsecure_channel.cpp
  
  src/easy_grpc/server/config.cpp
  src/easy_grpc/server/server.cpp
  src/easy_grpc/server/service.cpp
  
  src/easy_grpc/environment.cpp
  src/easy_grpc/completion_queue.cpp
)

target_compile_features(easy_grpc PUBLIC cxx_std_17)

target_include_directories(easy_grpc
  PUBLIC include
  PRIVATE src
)

target_link_libraries(easy_grpc
    grpc.a
    gpr.a
    protobuf::libprotobuf
    Threads::Threads
)


add_subdirectory(examples/00_hello_rpc)
enable_testing()
add_subdirectory(tests)