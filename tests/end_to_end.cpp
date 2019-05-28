#include "easy_grpc/easy_grpc.h"

#include "generated/test.egrpc.pb.h"

#include "gtest/gtest.h"

namespace rpc = easy_grpc;

class Test_sync_impl : public tests::TestService {
public:
  ::rpc::Future<::tests::TestReply> TestMethod(const ::tests::TestRequest& req) override {
    ::tests::TestReply result;
    result.set_name(req.name() + "_replied");

    return {result};
  }
};

TEST(test_easy_grpc, simple_rpc) {
  rpc::Environment grpc_env;

  rpc::Completion_queue server_queue;
  rpc::Completion_queue client_queue;

  Test_sync_impl sync_srv;

  int server_port = 0;
  rpc::server::Server server = rpc::server::Config()
    // Methods without any listenings queue will will use this set instead.
    .with_default_listening_queues({&server_queue, &server_queue + 1})

    // Use our service
    .with_service(&sync_srv)

    // Open an unsecured port
    .with_listening_port("127.0.0.1", {}, &server_port);

  EXPECT_NE(0, server_port);

  rpc::client::Unsecure_channel channel(std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);
  tests::TestService::Stub stub(&channel);

  ::tests::TestRequest req;
  req.set_name("dude");
  EXPECT_EQ(stub.TestMethod(req).get().name(), "dude_replied");


}