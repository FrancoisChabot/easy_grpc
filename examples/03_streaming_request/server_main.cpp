#include "easy_grpc/easy_grpc.h"
#include "generated/hello.egrpc.pb.h"

namespace rpc = easy_grpc;
namespace client = rpc::client;

using rpc::Future;
using rpc::Promise;

using pkg::HelloService;
using pkg::HelloRequest;
using pkg::HelloReply;

class Hello_impl {
public:
  Future<HelloReply> SpamedHello(::easy_grpc::Stream_future<::pkg::HelloRequest>) {
    HelloReply rep;

    Promise<HelloReply> prom;
    prom.set_value(rep);

    return prom.get_future();
  }
};

int main() {
  rpc::Environment grpc_env;

  // The server will handle messages on 4 threads
  std::vector<rpc::Completion_queue> server_cqs(4);

  Hello_impl service;

  rpc::server::Config cfg;
  cfg.add_default_listening_queues({server_cqs.begin(), server_cqs.end()})

    // Use our service
    .add_service(HelloService::get_config(service))

    // Open an unsecured port
    .add_listening_port("0.0.0.0:12345");

  rpc::server::Server server(std::move(cfg));  

  // Please replace this with proper signal handling (gpr might have what we need here...)
  while(1) {
    std::this_thread::sleep_for(std::chrono::minutes(2));
  }
  return 0;
}
