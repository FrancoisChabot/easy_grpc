#include "easy_grpc/easy_grpc.h"
#include "generated/data.egrpc.pb.h"

namespace rpc = easy_grpc;
namespace client = rpc::client;

using rpc::Future;

int main() {
  rpc::Environment grpc_env;
  {
    
    rpc::Completion_queue wp;
    {
      
      client::Unsecure_channel channel("localhost:12345", &wp);
      {
        sas::MyService::Stub stub(&channel);
        {

        // Call method
        auto sync =
            stub.MyMethod({}).then([](auto rep) { std::cout << rep.data() << "\n"; });

        std::cerr << "1\n";
        sync.get();
        
        }
        std::cerr << "2\n";
      }
      std::cerr << "3\n";
      
    }
    std::cerr << "4\n";
    
  }
  std::cerr << "5\n";
  return 0;
}