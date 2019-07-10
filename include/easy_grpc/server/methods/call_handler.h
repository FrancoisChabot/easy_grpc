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

#ifndef EASY_GRPC_SERVER_METHOD_CALL_HANDLER_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_CALL_HANDLER_H_INCLUDED

#include "easy_grpc/config.h"
#include "easy_grpc/error.h"
#include "easy_grpc/serialize.h"
#include "easy_grpc/function_traits.h"

#include "grpc/grpc.h"

#include <cassert>

namespace easy_grpc {
namespace server {
namespace detail {

// Unary calls are a bit special in the sense that we allow the handlers to be fully synchronous by returning
// a RepT (as opposed to a Future<RepT>)
class Call_handler : public Completion_callback {
 public:

  Call_handler() {
    grpc_metadata_array_init(&request_metadata_);
    grpc_metadata_array_init(&server_metadata_);
  }

  ~Call_handler() {
    if(call_) {
      grpc_call_unref(call_);
    }
    grpc_metadata_array_destroy(&request_metadata_);
    grpc_metadata_array_destroy(&server_metadata_);
  }


void op_send_message(grpc_op& op, grpc_byte_buffer* buffer) {
  op.op = GRPC_OP_SEND_MESSAGE;
  op.flags = 0;
  op.reserved = nullptr;
  op.data.send_message.send_message = buffer;
}

void op_send_status(grpc_op& op, grpc_status_code code = GRPC_STATUS_OK, grpc_slice* details = nullptr) {
  op.op = GRPC_OP_SEND_STATUS_FROM_SERVER;
  op.flags = 0;
  op.reserved = nullptr;
  op.data.send_status_from_server.trailing_metadata_count = 0;
  op.data.send_status_from_server.trailing_metadata = nullptr;
  op.data.send_status_from_server.status = code;
  op.data.send_status_from_server.status_details = details;
}

void op_recv_close(grpc_op& op) {
  op.op = GRPC_OP_RECV_CLOSE_ON_SERVER;
  op.flags = 0;
  op.reserved = nullptr;
  op.data.recv_close_on_server.cancelled = &cancelled_;
}

void op_send_metadata(grpc_op& op) {
  op.op = GRPC_OP_SEND_INITIAL_METADATA;
  op.flags = 0;
  op.reserved = 0;
  op.data.send_initial_metadata.count = server_metadata_.count;
  op.data.send_initial_metadata.metadata = server_metadata_.metadata;
  op.data.send_initial_metadata.maybe_compression_level.is_set = false;
}

void op_recv_message(grpc_op& op, grpc_byte_buffer** payload) {
  op.op = GRPC_OP_RECV_MESSAGE;
  op.flags = 0;
  op.reserved = 0;
  op.data.recv_message.recv_message = payload;
}

std::tuple<grpc_status_code, grpc_slice> get_error_details(std::exception_ptr error) {
  grpc_status_code status = GRPC_STATUS_UNKNOWN;
  grpc_slice details = grpc_empty_slice();

  try {
    std::rethrow_exception(error);
  } catch (Rpc_error& e) {
    status = e.code();
    details = grpc_slice_from_copied_string(e.what());
  } catch (std::exception& e) {
    details = grpc_slice_from_copied_string(e.what());
  }
  catch(...) { }

  return {status, details};
}

template<typename RepT>
void send_unary_response(const RepT& rep, bool with_metadata, std::bitset<4> flags) {
  std::array<grpc_op, 4> ops;

  std::size_t ops_count = 3;

  auto buffer = serialize(rep);

  op_send_message(ops[0], buffer);
  op_send_status(ops[1]);
  op_recv_close(ops[2]);

  if(with_metadata) {
    ++ops_count;
    op_send_metadata(ops[3]);
  }

  auto call_status =
      grpc_call_start_batch(call_, ops.data(), ops_count, completion_tag(flags).data, nullptr);

  grpc_byte_buffer_destroy(buffer);  

  if (call_status != GRPC_CALL_OK) {
    // There's not much we can do about this beyond logging it.
    assert(false);  // TODO: HANDLE THIS
  }
}

void send_failure(std::exception_ptr error, bool with_metadata, std::bitset<4> flags) {
  std::array<grpc_op, 3> ops;

  std::size_t ops_count = 2;
  auto [status, details] = get_error_details(error);

  op_send_status(ops[0], status, &details);
  op_recv_close(ops[1]);

  if(with_metadata) {
    ++ops_count;
    op_send_metadata(ops[2]);
  }

  auto call_status =
      grpc_call_start_batch(call_, ops.data(), ops_count, completion_tag(flags).data, nullptr);

  grpc_slice_unref(details);

  if (call_status != GRPC_CALL_OK) {
    // There's not much we can do about this beyond logging it.
    assert(false);  // TODO: HANDLE THIS
  }
}

  grpc_call* call_ = nullptr;

  //Request-related
  gpr_timespec deadline_;
  grpc_metadata_array request_metadata_;

  //Reply-related
  int cancelled_ = false;
  grpc_metadata_array server_metadata_;
};

}  // namespace detail
}  // namespace server
}  // namespace easy_grpc
#endif
