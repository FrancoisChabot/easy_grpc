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
#include "easy_grpc/server/methods/ops.h"

#include <cassert>
#include <iostream>

namespace easy_grpc {
namespace server {
namespace detail {

template <typename ReqT, typename RepT, bool sync>
class Client_streaming_call_handler : public Completion_callback {
  Stream_promise<ReqT> reader_prom_;
  
public:
  static constexpr bool immediate_payload = false;

  Client_streaming_call_handler() {
    grpc_metadata_array_init(&request_metadata_);
    grpc_metadata_array_init(&server_metadata_);
  }

  ~Client_streaming_call_handler() {
    grpc_metadata_array_destroy(&request_metadata_);
    grpc_metadata_array_destroy(&server_metadata_);
  }

  template<typename CbT>
  void perform(const CbT& cb) {      
    auto reply_fut = cb(reader_prom_.get_future());

    std::array<grpc_op, 2> ops;
    ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
    ops[0].flags = 0;
    ops[0].reserved = 0;
    ops[0].data.send_initial_metadata.count = server_metadata_.count;
    ops[0].data.send_initial_metadata.metadata = server_metadata_.metadata;
    ops[0].data.send_initial_metadata.maybe_compression_level.is_set = false;

    ops[1].op = GRPC_OP_RECV_MESSAGE;
    ops[1].flags = 0;
    ops[1].reserved = 0;
    ops[1].data.recv_message.recv_message = &payload_;

    auto call_status =
        grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

    if (call_status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(call_status) << "\n";
      assert(false);  // TODO: HANDLE THIS
    }

    reply_fut.finally([this](expected<RepT> rep) { 
      this->finish(std::move(rep)); 
    });
  }

  void finish(expected<RepT> rep) {
    if (rep.has_value()) {
      send_unary_response(call_, rep.value(), nullptr, &cancelled_, completion_tag(true));
    } else {
      // The call has failed.
      send_failure(call_, rep.error(), nullptr, &cancelled_, completion_tag(true));
    }
  }

  bool exec(bool, bool all_done) noexcept override {
    if(!all_done) {
      if(payload_) {
        auto raw_data = payload_;
        payload_ = nullptr;

        std::array<grpc_op, 1> ops;

        ops[0].op = GRPC_OP_RECV_MESSAGE;
        ops[0].flags = 0;
        ops[0].reserved = 0;
        ops[0].data.recv_message.recv_message = &payload_;

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

  int cancelled_ = false;
  grpc_call* call_ = nullptr;
  gpr_timespec deadline_;
  grpc_metadata_array request_metadata_;
  grpc_metadata_array server_metadata_;
  grpc_byte_buffer* payload_ = nullptr;
};


  
}
}
}
#endif
