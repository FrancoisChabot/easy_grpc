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

#ifndef EASY_GRPC_SERVER_METHOD_OPS_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_OPS_H_INCLUDED

#include "easy_grpc/config.h"
#include "easy_grpc/error.h"
#include "easy_grpc/serialize.h"
#include "easy_grpc/function_traits.h"
#include "easy_grpc/completion_queue.h"
#include "var_future/future.h"

#include "grpc/grpc.h"

#include <cassert>

namespace easy_grpc {
namespace server {
namespace detail {

template<typename RepT>
void send_unary_response(grpc_call* call, const RepT& rep, const grpc_metadata_array* server_metadata, int* cancelled, Completion_tag completion) {
  std::array<grpc_op, 4> ops;

  std::size_t ops_count = 3;

  auto buffer = serialize(rep);
  ops[0].op = GRPC_OP_SEND_MESSAGE;
  ops[0].flags = 0;
  ops[0].reserved = nullptr;
  ops[0].data.send_message.send_message = buffer;

  ops[1].op = GRPC_OP_SEND_STATUS_FROM_SERVER;
  ops[1].flags = 0;
  ops[1].reserved = nullptr;
  ops[1].data.send_status_from_server.trailing_metadata_count = 0;
  ops[1].data.send_status_from_server.trailing_metadata = nullptr;
  ops[1].data.send_status_from_server.status = GRPC_STATUS_OK;
  ops[1].data.send_status_from_server.status_details = nullptr;

  ops[2].op = GRPC_OP_RECV_CLOSE_ON_SERVER;
  ops[2].flags = 0;
  ops[2].reserved = nullptr;
  ops[2].data.recv_close_on_server.cancelled = cancelled;

  if(server_metadata) {
    ++ops_count;
    ops[3].op = GRPC_OP_SEND_INITIAL_METADATA;
    ops[3].flags = 0;
    ops[3].reserved = 0;
    ops[3].data.send_initial_metadata.count = server_metadata->count;
    ops[3].data.send_initial_metadata.metadata = server_metadata->metadata;
    ops[3].data.send_initial_metadata.maybe_compression_level.is_set = false;
  }


  auto call_status =
      grpc_call_start_batch(call, ops.data(), ops_count, completion.data, nullptr);

  grpc_byte_buffer_destroy(buffer);

  if (call_status != GRPC_CALL_OK) {
    // There's not much we can do about this beyond logging it.
    assert(false);  // TODO: HANDLE THIS
  }
}


inline void send_failure(grpc_call* call, std::exception_ptr error, const grpc_metadata_array* server_metadata, int* cancelled, Completion_tag completion) {
  std::array<grpc_op, 3> ops;

  std::size_t ops_count = 2;
  grpc_status_code status = GRPC_STATUS_UNKNOWN;
  grpc_slice details = grpc_empty_slice();

  // If we can extract an error code, and message, do so.
  try {
    std::rethrow_exception(error);
  } catch (Rpc_error& e) {
    status = e.code();
    details = grpc_slice_from_copied_string(e.what());
  } catch (std::exception& e) {
    details = grpc_slice_from_copied_string(e.what());
  }
  catch(...) { }

  ops[0].op = GRPC_OP_SEND_STATUS_FROM_SERVER;
  ops[0].flags = 0;
  ops[0].reserved = nullptr;
  ops[0].data.send_status_from_server.trailing_metadata_count = 0;
  ops[0].data.send_status_from_server.trailing_metadata = nullptr;
  ops[0].data.send_status_from_server.status = status;
  ops[0].data.send_status_from_server.status_details = &details;

  ops[1].op = GRPC_OP_RECV_CLOSE_ON_SERVER;
  ops[1].flags = 0;
  ops[1].reserved = nullptr;
  ops[1].data.recv_close_on_server.cancelled = cancelled;

  if(server_metadata) {
    ++ops_count;

    ops[2].op = GRPC_OP_SEND_INITIAL_METADATA;
    ops[2].flags = 0;
    ops[2].reserved = 0;
    ops[2].data.send_initial_metadata.count = server_metadata->count;
    ops[2].data.send_initial_metadata.metadata = server_metadata->metadata;
    ops[2].data.send_initial_metadata.maybe_compression_level.is_set = false;
  }


  auto call_status =
      grpc_call_start_batch(call, ops.data(), ops_count, completion.data, nullptr);

  grpc_slice_unref(details);

  if (call_status != GRPC_CALL_OK) {
    // There's not much we can do about this beyond logging it.
    assert(false);  // TODO: HANDLE THIS
  }
}

}  // namespace detail
}  // namespace server
}  // namespace easy_grpc
#endif
