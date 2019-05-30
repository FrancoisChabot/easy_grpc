#include "easy_grpc/easy_grpc.h"

#include "generated/test.egrpc.pb.h"

#include "gtest/gtest.h"

namespace rpc = easy_grpc;

namespace {
class Test_sync_impl : public tests::TestService {
public:
  ::rpc::Future<::tests::TestReply> TestMethod(const ::tests::TestRequest& req) override {
    ::tests::TestReply result;
    result.set_name(req.name() + "_replied");

    return {result};
  }
};
}

TEST(test_easy_grpc, simple_rpc) {
  rpc::Environment grpc_env;

  std::array<rpc::Completion_queue, 1> server_queues;
  rpc::Completion_queue client_queue;

  Test_sync_impl sync_srv;

  int server_port = 0;
  rpc::server::Server server = rpc::server::Config()
    .with_default_listening_queues({server_queues.begin(), server_queues.end()})
    .with_service(sync_srv)
    .with_listening_port("127.0.0.1:0", {}, &server_port);

  EXPECT_NE(0, server_port);

  {
    rpc::client::Unsecure_channel channel(std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);
    tests::TestService::Stub stub(&channel);

    ::tests::TestRequest req;
    req.set_name("dude");
    EXPECT_EQ(stub.TestMethod(req).get().name(), "dude_replied");
  }
}


TEST(test_easy_grpc, big_volume) {
  rpc::Environment grpc_env;

  constexpr int receiving_threads = 3;
  constexpr int sending_threads = 3;
  constexpr int rpcs_to_send = 10000;
  
  std::array<rpc::Completion_queue, receiving_threads> server_queues;
  std::array<rpc::Completion_queue, sending_threads> client_queues;

  Test_sync_impl sync_srv;

  int server_port = 0;
  rpc::server::Server server = rpc::server::Config()
    .with_default_listening_queues({server_queues.begin(), server_queues.end()})
    .with_service(sync_srv)
    .with_listening_port("127.0.0.1:0", {}, &server_port);

  EXPECT_NE(0, server_port);

  rpc::client::Unsecure_channel channel(std::string("127.0.0.1:") + std::to_string(server_port), nullptr);
  tests::TestService::Stub stub(&channel);

  ::tests::TestRequest req;
  req.set_name("dude");

  std::vector<rpc::Future<::tests::TestReply>> results;
  results.reserve(rpcs_to_send);

  for(int i = 0 ; i < rpcs_to_send; ++i) {
    rpc::client::Call_options options;
    options.completion_queue = &client_queues[i%sending_threads];
    results.emplace_back(stub.TestMethod(req, options));
  }

  for(auto& f : results) {
    EXPECT_EQ(f.get().name(), "dude_replied");
  }
}