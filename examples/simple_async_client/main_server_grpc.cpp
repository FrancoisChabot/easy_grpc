
#include <iostream>
#include <memory>
#include <string>

#include <grpcpp/grpcpp.h>

#include "generated/data.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// Logic and data behind the server's behavior.
class MyServiceImpl final : public ::sas::MyService::Service {
  ::grpc::Status MyMethod(::grpc::ServerContext* context,
                          const ::sas::MyRequest* request,
                          ::sas::MyResponse* response) override {
    response->set_data("hullo!");
    return Status::OK;
  }
};

void RunServer() {
  std::string server_address("0.0.0.0:12345");
  MyServiceImpl service;

  ServerBuilder builder;
  // Listen on the given address without any authentication mechanism.
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  // Register "service" as the instance through which we'll communicate with
  // clients. In this case it corresponds to an *synchronous* service.
  builder.RegisterService(&service);
  // Finally assemble the server.
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  // Wait for the server to shutdown. Note that some other thread must be
  // responsible for shutting down the server for this call to ever return.
  server->Wait();
}

int main(int argc, char** argv) {
  RunServer();

  return 0;
}