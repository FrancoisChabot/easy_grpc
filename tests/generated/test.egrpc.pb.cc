// This code was generated by the easy_grpc protoc plugin.

#include "test.egrpc.pb.h"

namespace tests {

// ********** TestService ********** //

namespace {
const char* kTestService_TestMethod_name = "/tests.TestService/TestMethod";
}

TestService::TestService() {
  TestMethod_method = ::easy_grpc::server::detail::make_unary_method<::tests::TestRequest, ::tests::TestReply>(kTestService_TestMethod_name, [this](::tests::TestRequest req) {
    return handle_TestMethod(std::move(req));
  });
}

TestService::Stub::Stub(::easy_grpc::client::Channel* c, ::easy_grpc::Completion_queue* default_queue)
  : channel_(c), default_queue_(default_queue ? default_queue : c->default_queue())
  , TestMethod_tag_(c->register_method(kTestService_TestMethod_name)) {}

// TestMethod
::easy_grpc::Future<::tests::TestReply> TestService::Stub::TestMethod(::tests::TestRequest req, ::easy_grpc::client::Call_options options) {
  if(!options.completion_queue) { options.completion_queue = default_queue_; }
  return ::easy_grpc::client::start_unary_call<::tests::TestReply>(channel_, TestMethod_tag_, std::move(req), std::move(options));
};

::easy_grpc::Future<::tests::TestReply> TestService::handle_TestMethod(::tests::TestRequest input) {
  try {
    return TestMethod(std::move(input));
  } catch(...) {
    return std::current_exception();
  }
}

void TestService::visit_methods(::easy_grpc::server::detail::Method_visitor& visitor) {
  visitor.visit(*TestMethod_method);
}
void TestService::start_listening_(const char* method_name, ::easy_grpc::Completion_queue* queue) {}
} // namespacetests

