#include "easy_grpc/easy_grpc.h"

#include "generated/test.egrpc.pb.h"
#include "gtest/gtest.h"

namespace rpc = easy_grpc;

namespace {
class Test_async_impl {
 public:
  using service_type = tests::TestBidirStreamingService;

  void TestMethod(::easy_grpc::Server_reader<::tests::TestRequest> req, ::easy_grpc::Server_writer<::tests::TestReply> rep)
    std::shared_ptr<int> count = std::make_shared<int>(0);

    return reader.for_each([count, rep](::tests::TestRequest) mutable {
        ::tests::TestReply reply;
        rep.push(reply);
        
      }).then([count, rep]() {
        rep.finish();
        return reply;
      });
  }
};
}

TEST(bidir_streaming, simple_call) {
  rpc::Environment env;

  std::array<rpc::Completion_queue, 1> server_queues;
  rpc::Completion_queue client_queue;
  
  
  Test_async_impl async_srv;

  int server_port = 0;
  rpc::server::Server server =
      rpc::server::Config()
          .with_default_listening_queues(
              {server_queues.begin(), server_queues.end()})
          .with_service(tests::TestClientStreamingService::get_config(async_srv))
          .with_listening_port("127.0.0.1:0", {}, &server_port);

  EXPECT_NE(0, server_port);
  
  rpc::client::Unsecure_channel channel(
        std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);

  tests::TestClientStreamingService::Stub stub(&channel);

  auto [req_stream, rep_fut] = stub.TestMethod();

  ::tests::TestRequest req;
  for(int i = 0 ; i < 6; ++i) {
    req_stream.push(req);
  }
  req_stream.finish();
  EXPECT_EQ(rep_fut.get().count(), 6);
}
