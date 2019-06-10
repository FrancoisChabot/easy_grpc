#include "easy_grpc/easy_grpc.h"

#include "packet.h"

namespace rpc = easy_grpc;

using rpc::Future;

class Blarg_service {
public:
  Future<Reply_packet> foo(Request_packet req) {
    Reply_packet result;
    
    result.a = req.a;
    result.b = req.a * 2;
    result.c = req.a * req.a;

    return Future<Reply_packet>{result};
  }

  Future<Reply_packet> bar(Request_packet req) {
    Reply_packet result;
    
    result.a = req.a;
    result.b = req.a * 4;
    result.c = req.a * req.a * req.a;

    return Future<Reply_packet>{result};
  }

  rpc::server::Service_config make_config() {
    rpc::server::Service_config blarg_config("test.Blarg");

    blarg_config.add_method("foo", [this](Request_packet req){return foo(std::move(req));});
    blarg_config.add_method("bar", [this](Request_packet req){return bar(std::move(req));});

    return blarg_config;
  }
};

int main() {
  rpc::Environment grpc_env;

  // The server will handle messages on 4 threads
  std::vector<rpc::Completion_queue> server_cqs(4);

  Blarg_service service;


  rpc::server::Server server = rpc::server::Config()
    .with_default_listening_queues({server_cqs.begin(), server_cqs.end()})
    .with_service(service.make_config())
    .with_listening_port("0.0.0.0:12345");


  // Please replace this with proper signal handling (gpr might have what we need here...)
  while(1) {
    std::this_thread::sleep_for(std::chrono::minutes(2));
  }
  return 0;
}
