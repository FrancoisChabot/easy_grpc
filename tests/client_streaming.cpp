#include "easy_grpc/easy_grpc.h"

#include "generated/test.egrpc.pb.h"
#include "gtest/gtest.h"

namespace rpc = easy_grpc;

namespace {
class Test_async_impl {
 public:
  using service_type = tests::TestClientStreamingService;

  ::easy_grpc::Future<::tests::TestReply> TestMethod(::easy_grpc::Stream_future<::tests::TestRequest> reader) {
    std::shared_ptr<int> count = std::make_shared<int>(0);
    return reader.for_each([count](::tests::TestRequest) mutable {
        *count += 1;
      }).then([count]() {
        ::tests::TestReply reply;
        reply.set_count(*count);
        return reply;
      });
  }
};
}

TEST(client_streaming, simple_call) {
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
  req.set_name("inc");
  for(int i = 0 ; i < 6; ++i) {
    req_stream.push(req);
  }
  req_stream.complete();
  EXPECT_EQ(rep_fut.get().count(), 6);
}
