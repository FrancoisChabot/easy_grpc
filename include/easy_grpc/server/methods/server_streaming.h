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
#include "easy_grpc/server/methods/call_handler.h"

#include <cassert>
#include <queue>
#include <iostream>

namespace easy_grpc {
namespace server {
namespace detail {

template <typename ReqT, typename RepT, bool sync>
class Server_streaming_call_handler : public Call_handler {
public:

  grpc_byte_buffer* payload_ = nullptr;
  static constexpr bool immediate_payload = true;
  
  template<typename CbT>
  void perform(const CbT& handler) {
    assert(this->payload_);
    
    auto req = deserialize<ReqT>(this->payload_);
    grpc_byte_buffer_destroy(this->payload_);

    batch_in_flight_ = true;

    std::array<grpc_op, 1> ops;
    op_send_metadata(ops[0]);
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
    
    grpc_op op;
    op_send_message(op, serialize(val));

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

    std::array<grpc_op, 2> ops;

    op_send_status(ops[0]);
    op_recv_close(ops[1]);

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag(true).data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
      assert(false);
    }
  }

  bool exec(bool, std::bitset<4> flags) noexcept override {
    if(flags.test(0)) {
      return true;
    }

    std::lock_guard l(mtx_);
    batch_in_flight_ = false;

    if(!pending_ops_.empty()) {
      flush_();
    } else if(finished_) {
      send_end();
    }
    return false;
  }

  void send_end() {
    std::array<grpc_op, 2> ops;

    op_recv_close(ops[0]);
    op_send_status(ops[1]);

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag(1).data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
      assert(false);
    }
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

  // I really wish we could do this without a mutex, somehow...
  std::mutex mtx_;
  bool finished_ = false;
  bool batch_in_flight_ = false;
  std::queue<grpc_op> pending_ops_;
};
}
}
}
#endif
