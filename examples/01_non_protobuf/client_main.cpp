#include "easy_grpc/easy_grpc.h"

#include "packet.h"

namespace rpc = easy_grpc;
namespace client = rpc::client;

// Note that nothing is forcing you to package the functions in a stub like this.
struct Blarg_stub {
  Blarg_stub(rpc::client::Channel* c) 
    : foo("/test.Blar/foo", c)
    , bar("/test.Blar/bar", c) {}

  rpc::client::Method_stub<Request_packet, Reply_packet> foo;
  rpc::client::Method_stub<Request_packet, Reply_packet> bar;
};

int main() {
  rpc::Environment grpc_env;
    
  rpc::Completion_queue wp;
  
  client::Unsecure_channel channel("localhost:12345", &wp);
  Blarg_stub stub(&channel);
  
  // Call method
  Request_packet req = {4};
  auto rep_fut = stub.foo(req);

  try {
    Reply_packet rep = rep_fut.get_std_future().get();
    std::cout << rep.a << " " << rep.b << " " << rep.c << "\n";
  }
  catch(std::exception& e) {
    std::cerr << "Something went wrong: " << e.what() << "\n";
  }
  return 0;
}
