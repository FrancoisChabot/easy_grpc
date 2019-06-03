cmake_minimum_required(VERSION 3.10)
project(easy_grpc VERSION 0.0.1)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

SET(EASY_GRPC_BUILD_EXAMPLES ON CACHE BOOL "easy_grpc examples")
SET(EASY_GRPC_TEST_COVERAGE OFF CACHE BOOL "easy_grpc Coverage")
SET(EASY_GRPC_BUILD_TESTS ON CACHE BOOL "easy_grpc tests")


if(EASY_GRPC_TEST_COVERAGE)
  if(CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    APPEND_COVERAGE_COMPILER_FLAGS()
  else()
    message( FATAL_ERROR "Code coverage only works on gcc" )
  endif()
endif()

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
    Threads::Threads
)

install(TARGETS easy_grpc
  EXPORT   sl_toolsTargets
  ARCHIVE  DESTINATION lib
  LIBRARY  DESTINATION lib
  RUNTIME  DESTINATION bin
)

if(EASY_GRPC_BUILD_EXAMPLES)
  find_package(Protobuf REQUIRED)

  add_subdirectory(examples/00_hello_rpc)
  add_subdirectory(examples/01_non_protobuf)
  add_subdirectory(examples/02_streaming_reply)
endif()

if(EASY_GRPC_BUILD_TESTS)
  include(CTest)
  add_subdirectory(tests)
endif()

#install cmake export
install(EXPORT easy_grpcTargets DESTINATION lib/cmake/easy_grpc)

configure_file(cmake/easy_grpc-config.cmake.in
  "${PROJECT_BINARY_DIR}/easy_grpc-config.cmake" @ONLY)
