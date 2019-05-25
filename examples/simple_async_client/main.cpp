#include "easy_grpc/easy_grpc.h"
#include "generated/data.egrpc.pb.h"

namespace rpc = easy_grpc;
namespace client = rpc::client;

using rpc::Future;

int main() {
  rpc::Environment grpc_env;
    
  rpc::Completion_queue wp;
  
  client::Unsecure_channel channel("localhost:12345", &wp);
  sas::MyService::Stub stub(&channel);
  
  // Call method
  auto sync =
    stub.MyMethod({}).then([](auto rep) { std::cout << rep.data() << "\n"; });

  sync.get();
  
   return 0;
}