cmake_minimum_required(VERSION 3.10)

project(easy_grpc VERSION 0.0.1)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(protoc_plugin)




add_library(easy_grpc
  src/easy_grpc/client/channel.cpp
  src/easy_grpc/client/unsecure_channel.cpp
  src/easy_grpc/environment.cpp
  src/easy_grpc/server.cpp
  src/easy_grpc/completion_queue.cpp
)

target_include_directories(easy_grpc
  PUBLIC include
  PRIVATE src
)

target_link_libraries(easy_grpc
    -Wl,--whole-archive grpc++_reflection.a -Wl,--no-whole-archive
    grpc++.a
    grpc.a
    gpr.a
    protobuf::libprotobuf
    Threads::Threads
)


add_subdirectory(examples/simple_async_client)
enable_testing()
add_subdirectory(tests)