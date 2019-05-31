add_executable(non_proto_server 
  server_main.cpp
)

add_executable(non_proto_client 
  client_main.cpp
)

target_include_directories(non_proto_server PRIVATE .)
target_include_directories(non_proto_client PRIVATE .)

target_link_libraries(non_proto_server easy_grpc grpc.a)
target_link_libraries(non_proto_client easy_grpc grpc.a)