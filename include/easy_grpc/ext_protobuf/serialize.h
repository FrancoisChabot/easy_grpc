// Copyright 2019 Age of Minds inc.

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef EASY_GRPC_EXT_PROTOBUF_SERIALIZE_INCLUDED_H
#define EASY_GRPC_EXT_PROTOBUF_SERIALIZE_INCLUDED_H

#include "easy_grpc/serialize.h"
#include <google/protobuf/message.h>

namespace easy_grpc {

template <typename T>
struct Serializer<
    T,
    std::enable_if_t<std::is_base_of_v<::google::protobuf::MessageLite, T>>> {
  static grpc_byte_buffer* serialize(const T& msg) {
    auto msg_size = msg.ByteSize();
    auto slice = grpc_slice_malloc(msg_size);

    msg.SerializeWithCachedSizesToArray(GRPC_SLICE_START_PTR(slice));

    
    return grpc_raw_byte_buffer_create(&slice, 1);
  }

  static T deserialize(grpc_byte_buffer* data) {
    assert(data->type == GRPC_BB_RAW);

    // TODO: be smarter about this...
    grpc_byte_buffer_reader reader;
    grpc_byte_buffer_reader_init(&reader, data);
    auto slice = grpc_byte_buffer_reader_readall(&reader);

    T result;

    result.ParseFromArray(GRPC_SLICE_START_PTR(slice),
                          GRPC_SLICE_LENGTH(slice));
    grpc_slice_unref(slice);
    grpc_byte_buffer_reader_destroy(&reader);

    return result;
  }
};
}  // namespace easy_grpc

#endif
