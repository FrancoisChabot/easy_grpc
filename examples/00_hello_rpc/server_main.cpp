#include "easy_grpc/easy_grpc.h"
#include "easy_grpc_reflection/reflection.h"
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
  // Says hello
  HelloReply SayHello(const HelloRequest& req) {
    std::cerr << "saying hello\n";
    if(req.name() == "") {
      throw rpc::error::invalid_argument("must provide name");
    }

    // This is actually a synchronous handler.
    // But all you need to do to make this asynchronous is to return a future
    // that is fullfilled at a later time.
    HelloReply rep;
    rep.set_greeting(std::string("Hello " + req.name()));

    return rep;
  }

  // Says Goodbye
  Future<HelloReply> SayBye(const HelloRequest& req) {
    if(req.name() == "") {
      throw rpc::error::invalid_argument("must provide name");
    }

    Promise<HelloReply> prom;
    auto result = prom.get_future();    

    std::thread handling_thread([prom = std::move(prom), req = std::move(req)]() mutable {
      HelloReply rep;
      rep.set_greeting(std::string("Goodbye " + req.name()));

      prom.set_value(std::move(rep));
    });

    return result;
  }
};

int main() {
  rpc::Environment grpc_env;

  // The server will handle messages on 4 threads
  std::vector<rpc::Completion_queue> server_cqs(4);

  Hello_impl service;

;

  rpc::server::Server server( rpc::server::Config()
    // 
    .add_default_listening_queues({server_cqs.begin(), server_cqs.end()})
    // Use our service
    .add_service(HelloService::get_config(service))

    // Open an unsecured port
    .add_listening_port("0.0.0.0:12345")

    // enable reflection
    .add_feature(easy_grpc::Reflection_feature()));

  // Please replace this with proper signal handling (gpr might have what we need here...)
  while(1) {
    std::this_thread::sleep_for(std::chrono::minutes(2));
  }
  return 0;
}
