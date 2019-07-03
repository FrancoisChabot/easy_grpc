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

#ifndef EASY_GRPC_SERVER_METHOD_SERVER_STREAMING_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_SERVER_STREAMING_H_INCLUDED

#include "easy_grpc/config.h"
#include "easy_grpc/server/methods/method.h"

#include "easy_grpc/function_traits.h"
#include "easy_grpc/serialize.h"

#include <cassert>
#include <queue>
#include <iostream>

namespace easy_grpc {
namespace server {
namespace detail {

template <typename ReqT, typename RepT, bool sync>
class Server_streaming_call_handler : public Completion_callback {
public:
  static constexpr bool immediate_payload = true;

  template<typename CbT>
  void perform(const CbT& handler) {
    assert(this->payload_);
    
    auto req = deserialize<ReqT>(this->payload_);
    grpc_byte_buffer_destroy(this->payload_);

    batch_in_flight_ = true;
    std::array<grpc_op, 1> ops;

    ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
    ops[0].flags = 0;
    ops[0].reserved = 0;
    ops[0].data.send_initial_metadata.count = server_metadata_.count;
    ops[0].data.send_initial_metadata.metadata = server_metadata_.metadata;
    ops[0].data.send_initial_metadata.maybe_compression_level.is_set = false;

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);


    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);

    try {
      handler(req).for_each([this](RepT rep) {
        push(rep);
      }).finally([this](expected<void> status) {
        if(status.has_value()) {
          finish();
        }
        else {
          fail(status.error());
        }
      });
    } catch(...) {
      fail(std::current_exception());
    }
  }


  void push(const RepT& val) {
    std::lock_guard l(mtx_);

    auto buffer = serialize(val);
    
    grpc_op op;
    op.op = GRPC_OP_SEND_MESSAGE;
    op.flags = 0;
    op.reserved = nullptr;
    op.data.send_message.send_message = buffer;

    pending_ops_.push(op);

    flush_();
  }

  void finish() {
    std::lock_guard l(mtx_);

    finished_ = true;

    if(!batch_in_flight_) {
      send_end();
    }
  }

  void fail(std::exception_ptr) {
    std::lock_guard l(mtx_);
  }

  bool exec(bool, bool) noexcept override {
    std::lock_guard l(mtx_);
    batch_in_flight_ = false;

    if(!pending_ops_.empty()) {
      flush_();
      return false;
    }

    if(finished_) {
      if(!end_sent_) {
        send_end();
      }
      else {
        return true;
      }
    }
    return false;
  }

  void send_end() {
    std::array<grpc_op, 2> ops;

    ops[0].op = GRPC_OP_SEND_STATUS_FROM_SERVER;
    ops[0].flags = 0;
    ops[0].reserved = nullptr;
    ops[0].data.send_status_from_server.trailing_metadata_count = 0;
    ops[0].data.send_status_from_server.trailing_metadata = nullptr;
    ops[0].data.send_status_from_server.status = GRPC_STATUS_OK;
    ops[0].data.send_status_from_server.status_details = nullptr;

    ops[1].op = GRPC_OP_RECV_CLOSE_ON_SERVER;
    ops[1].flags = 0;
    ops[1].reserved = nullptr;
    ops[1].data.recv_close_on_server.cancelled = &cancelled_;

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);

    end_sent_ = true;
  }

  void flush_() {
    if(batch_in_flight_) {
      return ;
    }

    batch_in_flight_ = true;

    auto op = pending_ops_.front();
    pending_ops_.pop();

    assert(op.op == GRPC_OP_SEND_MESSAGE);
    
    auto status =
      grpc_call_start_batch(call_, &op, 1, completion_tag().data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);
    
    grpc_byte_buffer_destroy(op.data.send_message.send_message);
  }


  grpc_byte_buffer* payload_ = nullptr;
  Future<void> finalize_fut_;
  int cancelled_ = false;
  grpc_call* call_ = nullptr;
  gpr_timespec deadline_;
  grpc_metadata_array request_metadata_;
  grpc_metadata_array server_metadata_;

  // I really wish we could do this without a mutex, somehow...
  std::mutex mtx_;
  bool end_sent_ = false;
  bool finished_ = false;
  bool batch_in_flight_ = false;
  std::queue<grpc_op> pending_ops_;
};
}
}
}
#endif
