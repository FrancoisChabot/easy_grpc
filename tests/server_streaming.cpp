#include "easy_grpc/easy_grpc.h"

#include "generated/test.egrpc.pb.h"
#include "gtest/gtest.h"

namespace rpc = easy_grpc;

namespace {
class Test_async_impl {
 public:
  using service_type = tests::TestServerStreamingService;

  ::rpc::Stream_future<::tests::TestReply> TestMethod(::tests::TestRequest) {
    ::tests::TestReply val;

    ::rpc::Stream_promise<::tests::TestReply> rep;
    auto result = rep.get_future();

    rep.push(val);
    rep.push(val);
    rep.push(val);
    rep.complete();
    return result;
  }
};
}

TEST(server_streaming, simple_call) {
  rpc::Environment env;

  std::array<rpc::Completion_queue, 1> server_queues;
  rpc::Completion_queue client_queue;
  
  
  Test_async_impl async_srv;

  int server_port = 0;
  rpc::server::Server server =
      rpc::server::Config()
          .add_default_listening_queues(
              {server_queues.begin(), server_queues.end()})
          .add_service(tests::TestServerStreamingService::get_config(async_srv))
          .add_listening_port("127.0.0.1:0", {}, &server_port);

  EXPECT_NE(0, server_port);
  
  rpc::client::Unsecure_channel channel(
        std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);

  tests::TestServerStreamingService::Stub stub(&channel);

  ::tests::TestRequest req;
  req.set_name("dude");

  auto rep_stream = stub.TestMethod(req);

  auto count = std::make_shared<int>(0);
  auto all_done = rep_stream.for_each([count](::tests::TestReply){
    ++*count;
  }).then([count](){ 
    return *count;
  });

  EXPECT_EQ(all_done.get(), 3);
}
