#include "easy_grpc/easy_grpc.h"

#include "generated/test.egrpc.pb.h"
#include "gtest/gtest.h"

namespace rpc = easy_grpc;

namespace {
class Test_async_impl {
 public:
  using service_type = tests::TestBidirStreamingService;

  ::easy_grpc::Stream_future<::tests::TestReply> TestMethod(::easy_grpc::Stream_future<::tests::TestRequest> req) {
    auto rep = std::make_shared<::easy_grpc::Stream_promise<::tests::TestReply>>();
    auto rep_fut = rep->get_future();

    req.for_each([rep](::tests::TestRequest) mutable {
      ::tests::TestReply reply;
      rep->push(reply);
    }).finally([rep](aom::expected<void> status) mutable {
      if(status.has_value()) {
        rep->complete();
      }
      else {
        rep->set_exception(status.error());
      }
    });

    return rep_fut;
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
          .with_service(tests::TestBidirStreamingService::get_config(async_srv))
          .with_listening_port("127.0.0.1:0", {}, &server_port);

  EXPECT_NE(0, server_port);
  
  rpc::client::Unsecure_channel channel(
        std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);

  tests::TestBidirStreamingService::Stub stub(&channel);

  auto [req_stream, rep_stream] = stub.TestMethod();

  auto count = std::make_shared<int>(0);
  auto all_done = rep_stream.for_each([count](::tests::TestReply){
    ++*count;
  }).then([count](){ 
    return *count;
  });

  ::tests::TestRequest req;
  for(int i = 0 ; i < 6; ++i) {
    req_stream.push(req);
  }
  req_stream.complete();
  EXPECT_EQ(all_done.get(), 6);
}
