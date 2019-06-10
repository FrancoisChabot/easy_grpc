#include "easy_grpc/easy_grpc.h"

#include "generated/test.egrpc.pb.h"
#include "gtest/gtest.h"

namespace rpc = easy_grpc;

namespace {
class Test_sync_impl {
 public:
  using service_type = tests::TestService;

  ::tests::TestReply TestMethod(::tests::TestRequest req) {
    ::tests::TestReply result;
    result.set_name(req.name() + "_replied");

    return result;
  }
};

class Failing_impl : public tests::TestService {
 public:
  ::rpc::Future<::tests::TestReply> TestMethod(
      ::tests::TestRequest) override {
    throw rpc::error::unimplemented("not done");
  }
};

class Junk_throwing_impl : public tests::TestService {
 public:
  ::rpc::Future<::tests::TestReply> TestMethod(
      ::tests::TestRequest) override {
    throw 12;
  }
};

class Failure_returning_impl : public tests::TestService {
 public:
  ::rpc::Future<::tests::TestReply> TestMethod(
      ::tests::TestRequest) override {
    rpc::Promise<::tests::TestReply> prom;
    auto result = prom.get_future();
    try {
      throw rpc::error::unimplemented("not done");
    } catch (...) {
      prom.set_exception(std::current_exception());
    }

    return result;
  }
};
}  // namespace

TEST(server, bind_failure) {
  rpc::Environment env;

  std::array<rpc::Completion_queue, 1> server_queues;

  Test_sync_impl sync_srv;

  auto cfg = rpc::server::Config();

  cfg.with_default_listening_queues(
         {server_queues.begin(), server_queues.end()})
      .with_service(sync_srv)
      .with_listening_port("[", {});

  EXPECT_THROW(rpc::server::Server(std::move(cfg)), std::runtime_error);
}

TEST(server, failing_call) {
  rpc::Environment env;

  std::array<rpc::Completion_queue, 1> server_queues;
  rpc::Completion_queue client_queue;

  Failing_impl failing_srv;

  auto cfg = rpc::server::Config();

  int server_port = 0;
  cfg.with_default_listening_queues(
         {server_queues.begin(), server_queues.end()})
      .with_service(failing_srv)
      .with_listening_port("127.0.0.1:0", {}, &server_port);

  rpc::server::Server srv(cfg);

  rpc::client::Unsecure_channel channel(
      std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);
  tests::TestService::Stub stub(&channel);

  ::tests::TestRequest req;
  req.set_name("dude");

  EXPECT_THROW(stub.TestMethod(req).get_std_future().get(), rpc::Rpc_error);
}

TEST(server, failing_with_junk) {
  rpc::Environment env;

  std::array<rpc::Completion_queue, 1> server_queues;
  rpc::Completion_queue client_queue;

  Junk_throwing_impl failing_srv;

  auto cfg = rpc::server::Config();

  int server_port = 0;
  cfg.with_default_listening_queues(
         {server_queues.begin(), server_queues.end()})
      .with_service(failing_srv)
      .with_listening_port("127.0.0.1:0", {}, &server_port);

  rpc::server::Server srv(cfg);

  rpc::client::Unsecure_channel channel(
      std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);
  tests::TestService::Stub stub(&channel);

  ::tests::TestRequest req;
  req.set_name("dude");

  EXPECT_THROW(stub.TestMethod(req).get_std_future().get(), rpc::Rpc_error);
}

TEST(server, failing_async_call) {
  rpc::Environment env;

  std::array<rpc::Completion_queue, 1> server_queues;
  rpc::Completion_queue client_queue;

  Failure_returning_impl failing_srv;

  auto cfg = rpc::server::Config();

  int server_port = 0;
  cfg.with_default_listening_queues(
         {server_queues.begin(), server_queues.end()})
      .with_service(failing_srv)
      .with_listening_port("127.0.0.1:0", {}, &server_port);

  rpc::server::Server srv(cfg);

  rpc::client::Unsecure_channel channel(
      std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);
  tests::TestService::Stub stub(&channel);

  ::tests::TestRequest req;
  req.set_name("dude");

  EXPECT_THROW(stub.TestMethod(req).get_std_future().get(), rpc::Rpc_error);
}

TEST(server, move_server) {
  rpc::Environment env;

  std::array<rpc::Completion_queue, 1> server_queues;

  Test_sync_impl sync_srv;

  auto cfg = rpc::server::Config();

  int server_port = 0;
  cfg.with_default_listening_queues(
         {server_queues.begin(), server_queues.end()})
      .with_service(sync_srv)
      .with_listening_port("127.0.0.1:0", {}, &server_port);

  ::tests::TestRequest req;
  req.set_name("dude");

  rpc::Completion_queue client_queue;
  rpc::client::Unsecure_channel channel;
  std::unique_ptr<tests::TestService::Stub> stub;

  {
    rpc::server::Server dummy;
    {
      rpc::server::Server srv(cfg);

      channel = rpc::client::Unsecure_channel(
          std::string("127.0.0.1:") + std::to_string(server_port),
          &client_queue);
      stub = std::make_unique<tests::TestService::Stub>(&channel);

      EXPECT_EQ(stub->TestMethod(req).get_std_future().get().name(), "dude_replied");

      rpc::server::Server moved_srv(std::move(srv));
      EXPECT_EQ(stub->TestMethod(req).get_std_future().get().name(), "dude_replied");

      dummy = std::move(moved_srv);
    }
    EXPECT_EQ(stub->TestMethod(req).get_std_future().get().name(), "dude_replied");
  }
  EXPECT_THROW(stub->TestMethod(req).get_std_future().get(), rpc::Rpc_error);
}
