find_package(Protobuf REQUIRED)

add_executable(easy_grpc_protoc_plugin main.cpp)
target_link_libraries(easy_grpc_protoc_plugin protobuf::libprotoc protobuf::libprotobuf stdc++fs Threads::Threads)
target_compile_features(easy_grpc_protoc_plugin PRIVATE cxx_std_17)
