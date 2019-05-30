#include "easy_grpc/easy_grpc.h"

#include "generated/test.egrpc.pb.h"

#include "gtest/gtest.h"

namespace rpc = easy_grpc;

struct Request_packet {
  int a;
};

struct Reply_packet {
  int a;
  int b;
  int c;
};

namespace easy_grpc {
template<>
struct Serializer<Request_packet> {
  static grpc_byte_buffer* serialize(const Request_packet& msg) {
    auto slice = grpc_slice_malloc(sizeof(Request_packet));
    std::memcpy(GRPC_SLICE_START_PTR(slice), &msg, sizeof(Request_packet));
    return grpc_raw_byte_buffer_create(&slice, 1);
  }

  static Request_packet deserialize(grpc_byte_buffer* data) {
    grpc_byte_buffer_reader reader;
    grpc_byte_buffer_reader_init(&reader, data);
    auto slice = grpc_byte_buffer_reader_readall(&reader);

    Request_packet result;

    std::memcpy(&result, GRPC_SLICE_START_PTR(slice), GRPC_SLICE_LENGTH(slice));

    grpc_slice_unref(slice);
    grpc_byte_buffer_reader_destroy(&reader);

    return result;
  }
};

template<>
struct Serializer<Reply_packet> {
  static grpc_byte_buffer* serialize(const Reply_packet& msg) {
    auto slice = grpc_slice_malloc(sizeof(Reply_packet));
    std::memcpy(GRPC_SLICE_START_PTR(slice), &msg, sizeof(Reply_packet));
    return grpc_raw_byte_buffer_create(&slice, 1);
  }

  static Reply_packet deserialize(grpc_byte_buffer* data) {
    grpc_byte_buffer_reader reader;
    grpc_byte_buffer_reader_init(&reader, data);
    auto slice = grpc_byte_buffer_reader_readall(&reader);

    Reply_packet result;

    std::memcpy(&result, GRPC_SLICE_START_PTR(slice), GRPC_SLICE_LENGTH(slice));

    grpc_slice_unref(slice);
    grpc_byte_buffer_reader_destroy(&reader);

    return result;
  }
};
}

class Custom_service {
public:

  rpc::Future<Reply_packet> DoWork(const Request_packet& req) {
    Reply_packet result = {req.a, req.a * 2, req.a * req.a};
    return {result};
  }

  class Stub {
  public:
    Stub(rpc::client::Channel* c) 
      : DoWork("/test.test/DoWork", c) {}

    rpc::client::Method_stub<Request_packet, Reply_packet> DoWork;
  };

  rpc::server::Service_config make_config() {
    rpc::server::Service_config blarg_config("test");

    blarg_config.add_method<Request_packet, Reply_packet>("/test.test/DoWork", [this](Request_packet req){return DoWork(std::move(req));});

    return blarg_config;
  }
};


TEST(binary_protocol, simple_rpc) {
  
  rpc::Environment grpc_env;

  std::array<rpc::Completion_queue, 1> server_queues;
  rpc::Completion_queue client_queue;

  Custom_service sync_srv;

  int server_port = 0;
  rpc::server::Server server = rpc::server::Config()
    .with_default_listening_queues({server_queues.begin(), server_queues.end()})
    .with_service(sync_srv.make_config())
    .with_listening_port("127.0.0.1:0", {}, &server_port);

  EXPECT_NE(0, server_port);

  {
    rpc::client::Unsecure_channel channel(std::string("127.0.0.1:") + std::to_string(server_port), &client_queue);
    Custom_service::Stub stub(&channel);

    auto result = stub.DoWork({4}).get();

    EXPECT_EQ(result.a, 4);
    EXPECT_EQ(result.b, 8);
    EXPECT_EQ(result.c, 16);
  }
  
}
