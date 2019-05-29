#ifndef EXAMPLES_01_PACKET_INCLUDED_H_
#define EXAMPLES_01_PACKET_INCLUDED_H_

#include "easy_grpc/easy_grpc.h"

#include <cstring>

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

#endif