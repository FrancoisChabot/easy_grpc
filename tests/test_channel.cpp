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

  std::array<rpc::Completion_queue, 1> queues;
};
}

TEST(channel, simple_connection) {
  rpc::Environment env;
  
  ::tests::TestRequest req;
  req.set_name("dude");

  Test_sync_impl sync_srv;
  int server_port = 0;
  rpc::server::Server srv(rpc::server::Config()
    .with_default_listening_queues({sync_srv.queues.begin(), sync_srv.queues.end()})
    .with_service(sync_srv)
    .with_listening_port("127.0.0.1", {}, &server_port)
  );

  rpc::Completion_queue client_queue;
  rpc::client::Unsecure_channel channel(std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);
  tests::TestService::Stub stub(&channel);

  EXPECT_EQ(stub.TestMethod(req).get().name(), "dude_replied");
}


TEST(channel, delete_from_base_class) {
  rpc::Environment env;
  
  ::tests::TestRequest req;
  req.set_name("dude");

  Test_sync_impl sync_srv;
  int server_port = 0;
  rpc::server::Server srv(rpc::server::Config()
    .with_default_listening_queues({sync_srv.queues.begin(), sync_srv.queues.end()})
    .with_service(sync_srv)
    .with_listening_port("127.0.0.1", {}, &server_port)
  );

  rpc::Completion_queue client_queue;
  std::unique_ptr<rpc::client::Channel> channel = std::make_unique<rpc::client::Unsecure_channel>(std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);
  tests::TestService::Stub stub(channel.get());

  EXPECT_EQ(stub.TestMethod(req).get().name(), "dude_replied");


  auto p = std::make_unique<rpc::client::Channel>(); 
}

TEST(channel, move_channel) {
  rpc::Environment env;
  
  ::tests::TestRequest req;
  req.set_name("dude");

  Test_sync_impl sync_srv;
  int server_port = 0;
  rpc::server::Server srv(rpc::server::Config()
    .with_default_listening_queues({sync_srv.queues.begin(), sync_srv.queues.end()})
    .with_service(sync_srv)
    .with_listening_port("127.0.0.1", {}, &server_port)
  );

  rpc::Completion_queue client_queue;
  rpc::client::Unsecure_channel blank_channel;
  {
    rpc::client::Unsecure_channel channel(std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);

    EXPECT_EQ(channel.default_queue(), &client_queue);

    rpc::client::Unsecure_channel moved_channel(std::move(channel));

    EXPECT_EQ(moved_channel.default_queue(), &client_queue);
    EXPECT_EQ(channel.default_queue(), nullptr);

    EXPECT_EQ(nullptr, channel.handle());

    blank_channel = std::move(moved_channel);
    EXPECT_EQ(nullptr, moved_channel.handle());
  }

  EXPECT_EQ(blank_channel.default_queue(), &client_queue);
  EXPECT_NE(nullptr, blank_channel.handle());

  tests::TestService::Stub stub(&blank_channel);
  EXPECT_EQ(stub.TestMethod(req).get().name(), "dude_replied");
}
