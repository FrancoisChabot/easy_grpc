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

#ifndef EASY_GRPC_SERVER_METHOD_BIDIR_STREAMING_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_BIDIR_STREAMING_H_INCLUDED

#include "easy_grpc/config.h"
#include "easy_grpc/server/methods/method.h"

#include "easy_grpc/function_traits.h"
#include "easy_grpc/serialize.h"
#include "easy_grpc/server/methods/call_handler.h"

#include <cassert>
#include <queue>

namespace easy_grpc {
namespace server {
namespace detail {


template <typename ReqT, typename RepT, bool sync>
class Bidir_streaming_call_handler : public Call_handler {
  Stream_promise<ReqT> reader_prom_;
  grpc_byte_buffer* payload_ = nullptr;
  
  std::mutex mtx_;
  bool finished_ = false;
  bool ready_to_send_ = false;
  std::queue<grpc_op> pending_ops_;

public:
  static constexpr bool immediate_payload = false;

  Bidir_streaming_call_handler() = default;
  ~Bidir_streaming_call_handler() {}

  template<typename CbT>
  void perform(const CbT& cb) {      
    auto reply_fut = cb(reader_prom_.get_future());

    reply_fut.for_each([this, cb](RepT rep) mutable {
      push(rep);
    }).finally([this](expected<void> status){
      if(status.has_value()) {
          finish();
        }
        else {
          fail(status.error());
        }
    });

    {
      std::array<grpc_op, 1> ops;
      op_send_metadata(ops[0]);

      auto call_status =
          grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag(8).data, nullptr);
      
      if (call_status != GRPC_CALL_OK) {
        std::cerr << grpc_call_error_to_string(call_status) << "\n";
        assert(false);  // TODO: HANDLE THIS
      }
    }
  }

  void send_server_end() {
    std::array<grpc_op, 2> ops;

    op_recv_close(ops[0]);
    op_send_status(ops[1]);

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag(4).data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
      assert(false);
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

    if(ready_to_send_) {
      send_server_end();
    }
  }

  void fail(std::exception_ptr) {
    
    std::lock_guard l(mtx_);

    std::array<grpc_op, 2> ops;

    op_send_status(ops[0]);
    op_recv_close(ops[1]);

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
      assert(false);
    }
  }

   void flush_() {
    if(!ready_to_send_) {
      return ;
    }

    ready_to_send_ = false;

    auto op = pending_ops_.front();
    pending_ops_.pop();

    assert(op.op == GRPC_OP_SEND_MESSAGE);
    
    auto status =
      grpc_call_start_batch(call_, &op, 1, completion_tag(2).data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);
    
    grpc_byte_buffer_destroy(op.data.send_message.send_message);
  }

  bool exec(bool, std::bitset<4> flags) noexcept override {
    std::unique_lock l(mtx_);

    bool recv_op = flags.test(0);
    bool send_op = flags.test(1);
    bool closing = flags.test(2);
    bool handshake = flags.test(3);

    if(handshake) {
      // start sending
      ready_to_send_ = true;
      if(!pending_ops_.empty()) {
        flush_();
      }

      // start receiving
      std::array<grpc_op, 1> ops;
        op_recv_message(ops[0], &payload_);
        
        auto call_status =
            grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag(1).data, nullptr);
        if (call_status != GRPC_CALL_OK) {
          assert(false);
        }
    }

    
    if(send_op) {
      ready_to_send_ = true;
      if(!pending_ops_.empty()) {
        flush_();
      }
      else if(finished_) {
        send_server_end();
      }
    }

    if(recv_op) {
      if(payload_) {
        auto raw_data = payload_;
          payload_ = nullptr;

          std::array<grpc_op, 1> ops;
          op_recv_message(ops[0], &payload_);
          
          auto call_status =
              grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag(1).data, nullptr);
          if (call_status != GRPC_CALL_OK) {
            assert(false);
          }

          l.unlock();
          reader_prom_.push(deserialize<ReqT>(raw_data));
          grpc_byte_buffer_destroy(raw_data);        
      }
      else {
        l.unlock();
        reader_prom_.complete();
      }
    }

    return closing;
  }
};

}
}
}
#endif
