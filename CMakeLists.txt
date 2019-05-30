cmake_minimum_required(VERSION 3.10)

project(easy_grpc VERSION 0.0.1)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

SET(COVERAGE OFF CACHE BOOL "Coverage")


set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)
if(CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0")
    set(COVERAGE_LCOV_EXCLUDES 
      '${PROJECT_SOURCE_DIR}/include/easy_grpc/variadic_future.h' 
      '/usr/*'
    )
    APPEND_COVERAGE_COMPILER_FLAGS()
endif()

include(CTest)

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

if (COVERAGE)
  target_compile_options(easy_grpc PUBLIC --coverage)
  target_link_libraries(easy_grpc gcov)
endif()

target_compile_features(easy_grpc PUBLIC cxx_std_17)

target_include_directories(easy_grpc
  PUBLIC include
  PRIVATE src
)

target_link_libraries(easy_grpc
    grpc.a
    gpr.a
    Threads::Threads
)


add_subdirectory(examples/00_hello_rpc)
add_subdirectory(examples/01_non_protobuf)



add_subdirectory(tests)
