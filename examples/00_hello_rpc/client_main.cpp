#include "easy_grpc/easy_grpc.h"
#include "generated/hello.egrpc.pb.h"

namespace rpc = easy_grpc;
namespace client = rpc::client;

using pkg::HelloService;
using pkg::HelloRequest;
using pkg::HelloReply;

int main() {
  rpc::Environment grpc_env;
    
  rpc::Completion_queue wp;
  
  client::Unsecure_channel channel("localhost:12345", &wp);
  HelloService::Stub stub(&channel);
  
  // Call method
  HelloRequest req;
  req.set_name("Frank");
  auto rep = stub.SayHello(req);

  try {
    std::cout << rep.get_std_future().get().greeting() << "\n";
  }
  catch(std::exception& e) {
    std::cerr << "Something went wrong: " << e.what() << "\n";
  }
  return 0;
}
