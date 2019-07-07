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

#ifndef EASY_GRPC_SERVER_METHOD_CLIENT_STREAMING_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_CLIENT_STREAMING_H_INCLUDED

#include "easy_grpc/config.h"

#include "easy_grpc/error.h"
#include "easy_grpc/server/methods/method.h"
#include "easy_grpc/function_traits.h"
#include "easy_grpc/server/methods/call_handler.h"

#include <cassert>
#include <iostream>

namespace easy_grpc {
namespace server {
namespace detail {

template <typename ReqT, typename RepT, bool sync>
class Client_streaming_call_handler : public Call_handler {
  Stream_promise<ReqT> reader_prom_;
  grpc_byte_buffer* payload_ = nullptr;
  
public:
  static constexpr bool immediate_payload = false;

  Client_streaming_call_handler() = default;
  ~Client_streaming_call_handler() {}

  template<typename CbT>
  void perform(const CbT& cb) {      
    auto reply_fut = cb(reader_prom_.get_future());

    std::array<grpc_op, 2> ops;
    op_send_metadata(ops[0]);
    op_recv_message(ops[1], &payload_);

    auto call_status =
        grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

    if (call_status != GRPC_CALL_OK) {
      assert(false);  // TODO: HANDLE THIS
    }

    reply_fut.finally([this](expected<RepT> rep) { 
      this->finish(std::move(rep)); 
    });
  }

  void finish(expected<RepT> rep) {
    if (rep.has_value()) {
      send_unary_response(rep.value(), false, true);
    } else {
      send_failure(rep.error(), false, true);
    }
  }

  bool exec(bool, bool all_done) noexcept override {
    if(!all_done) {
      if(payload_) {
        auto raw_data = payload_;
        payload_ = nullptr;

        std::array<grpc_op, 1> ops;
        op_recv_message(ops[0], &payload_);
        
        auto call_status =
            grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

        if (call_status != GRPC_CALL_OK) {
          assert(false);
        }

        reader_prom_.push(deserialize<ReqT>(raw_data));
        grpc_byte_buffer_destroy(raw_data);        
      }
      else {
        reader_prom_.complete();
      }
    }

    return all_done;
  }
};
}
}
}
#endif
