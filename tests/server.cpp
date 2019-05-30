#include "easy_grpc/easy_grpc.h"

#include "gtest/gtest.h"
#include "generated/test.egrpc.pb.h"

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

TEST(server, bind_failure) {
  rpc::Environment env;
  
  std::array<rpc::Completion_queue, 1> server_queues;

  Test_sync_impl sync_srv;

  auto cfg = rpc::server::Config();

  cfg.with_default_listening_queues({server_queues.begin(), server_queues.end()})
    .with_service(sync_srv)
    .with_listening_port("[", {});

  EXPECT_THROW( rpc::server::Server(std::move(cfg)), std::runtime_error);
}


TEST(server, move_server) {
  rpc::Environment env;
  
  std::array<rpc::Completion_queue, 1> server_queues;


    

  Test_sync_impl sync_srv;

  auto cfg = rpc::server::Config();

  int server_port = 0;
  cfg.with_default_listening_queues({server_queues.begin(), server_queues.end()})
    .with_service(sync_srv)
    .with_listening_port("127.0.0.1", {}, &server_port);

  ::tests::TestRequest req;
  req.set_name("dude");


  rpc::Completion_queue client_queue;
  rpc::client::Unsecure_channel channel;
  std::unique_ptr<tests::TestService::Stub> stub;

  {
    

    rpc::server::Server dummy;
    {
      rpc::server::Server srv(cfg);

      channel = rpc::client::Unsecure_channel(std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);
      stub = std::make_unique<tests::TestService::Stub>(&channel);

      EXPECT_EQ(stub->TestMethod(req).get().name(), "dude_replied");


      rpc::server::Server moved_srv(std::move(srv));
      EXPECT_EQ(stub->TestMethod(req).get().name(), "dude_replied");
      
      dummy = std::move(moved_srv);
    }
    EXPECT_EQ(stub->TestMethod(req).get().name(), "dude_replied");
  }
  EXPECT_THROW(stub->TestMethod(req).get(), rpc::Rpc_error);
}
