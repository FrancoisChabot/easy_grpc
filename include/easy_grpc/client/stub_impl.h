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

#ifndef EASY_GRPC_CLIENT_STUB_IMPL_INCLUDED_H
#define EASY_GRPC_CLIENT_STUB_IMPL_INCLUDED_H

#include "easy_grpc/completion_queue.h"
#include "easy_grpc/error.h"
#include "easy_grpc/serialize.h"
#include "easy_grpc/third_party/variadic_future.h"
#include "easy_grpc/client/channel.h"

#include "grpc/grpc.h"
#include "grpc/support/alloc.h"

#include <cstring>

namespace easy_grpc {

namespace client {
struct Call_options {
  Completion_queue* completion_queue = nullptr;
  gpr_timespec deadline = gpr_inf_future(GPR_CLOCK_REALTIME);
};

namespace detail {
template <typename RepT>
class Call_completion final : public Completion_queue::Completion {
 public:
  Call_completion(grpc_call* call) : call_(call) {
    grpc_metadata_array_init(&trailing_metadata_);
    grpc_metadata_array_init(&server_metadata_);
  }

  ~Call_completion() {
    grpc_metadata_array_destroy(&server_metadata_);
    grpc_metadata_array_destroy(&trailing_metadata_);

    if (recv_buffer_) {
      grpc_byte_buffer_destroy(recv_buffer_);
    }

    grpc_call_unref(call_);
  }

  void fail() {
    try {
      throw error::internal("failed to start call");
    } catch (...) {
      rep_.set_exception(std::current_exception());
    }
  }

  bool exec(bool success) noexcept override {
    if (status_ == GRPC_STATUS_OK) {
      rep_.set_value(deserialize<RepT>(recv_buffer_));
    } else {
      try {
        auto str = grpc_slice_to_c_string(status_details_);
        auto err = Rpc_error(status_, str);
        gpr_free(str);
        throw err;
      } catch (...) {
        rep_.set_exception(std::current_exception());
      }
    }
    return true;
  }

  grpc_call* call_;
  grpc_metadata_array server_metadata_;
  Promise<RepT> rep_;
  grpc_byte_buffer* recv_buffer_;

  grpc_metadata_array trailing_metadata_;
  grpc_status_code status_;
  grpc_slice status_details_;
  const char* error_string_;
};
}  // namespace detail

template <typename RepT, typename ReqT>
Future<RepT> start_unary_call(Channel* channel, void* tag, const ReqT& req,
                              Call_options options) {
  assert(options.completion_queue);

  auto call = grpc_channel_create_registered_call(
      channel->handle(), nullptr, GRPC_PROPAGATE_DEFAULTS,
      options.completion_queue->handle(), tag, options.deadline, nullptr);
  auto completion = new detail::Call_completion<RepT>(call);
  auto buffer = serialize(req);

  std::array<grpc_op, 6> ops;

  ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
  ops[0].flags = 0;
  ops[0].reserved = nullptr;
  ops[0].data.send_initial_metadata.count = 0;
  ops[0].data.send_initial_metadata.maybe_compression_level.is_set = 0;

  ops[1].op = GRPC_OP_SEND_MESSAGE;
  ops[1].flags = 0;
  ops[1].reserved = nullptr;
  ops[1].data.send_message.send_message = buffer;

  ops[2].op = GRPC_OP_RECV_INITIAL_METADATA;
  ops[2].flags = 0;
  ops[2].reserved = 0;
  ops[2].data.recv_initial_metadata.recv_initial_metadata =
      &completion->server_metadata_;

  ops[3].op = GRPC_OP_RECV_MESSAGE;
  ops[3].flags = 0;
  ops[3].reserved = 0;
  ops[3].data.recv_message.recv_message = &completion->recv_buffer_;

  ops[4].op = GRPC_OP_SEND_CLOSE_FROM_CLIENT;
  ops[4].flags = 0;
  ops[4].reserved = 0;

  ops[5].op = GRPC_OP_RECV_STATUS_ON_CLIENT;
  ops[5].flags = 0;
  ops[5].reserved = 0;
  ops[5].data.recv_status_on_client.trailing_metadata =
      &completion->trailing_metadata_;
  ops[5].data.recv_status_on_client.status = &completion->status_;
  ops[5].data.recv_status_on_client.status_details =
      &completion->status_details_;
  ops[5].data.recv_status_on_client.error_string = &completion->error_string_;

  auto result = completion->rep_.get_future();
  auto status =
      grpc_call_start_batch(call, ops.data(), ops.size(), completion, nullptr);

  if (status != GRPC_CALL_OK) {
    completion->fail();
    delete completion;
  }

  grpc_byte_buffer_destroy(buffer);

  return result;
}
}  // namespace client
}  // namespace easy_grpc
#endif