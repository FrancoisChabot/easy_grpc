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

#ifndef EASY_GRPC_SERVER_METHOD_LISTENER_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_LISTENER_H_INCLUDED

#include "easy_grpc/completion_queue.h"

#include <iostream>
namespace easy_grpc {
namespace server {
namespace detail {

template <typename CbT, typename HandlerT>
class Method_listener : public Completion_callback {
    using handler_type = HandlerT;
 public:
  Method_listener(grpc_server* server, void* registration,
                      grpc_completion_queue* cq, CbT cb)
      : srv_(server), reg_(registration), cq_(cq), cb_(std::move(cb)) {
    // It's really important that inject is not called here. As the object
    // could end up being deleted before it's fully constructed.
  }

  ~Method_listener() {
    if (pending_call_) {
      delete pending_call_;
    }
  }

  bool exec(bool success, std::bitset<4>) noexcept override {
    EASY_GRPC_TRACE(Method_listener, exec);

    if (success) {
      pending_call_->perform(cb_);
      pending_call_ = nullptr;

      // Listen for a new call.
      inject();
      return false;  // This object is recycled.
    }

    return true;
  }

  void inject() {
    EASY_GRPC_TRACE(Method_listener, inject);

    assert(pending_call_ == nullptr);

    pending_call_ = new handler_type;

    grpc_call_error status;
    if constexpr (handler_type::immediate_payload) {
      status = grpc_server_request_registered_call(
        srv_, reg_, &pending_call_->call_, &pending_call_->deadline_,
        &pending_call_->request_metadata_, &pending_call_->payload_, cq_, cq_,
        this);
    }
    else {

      status = grpc_server_request_registered_call(
        srv_, reg_, &pending_call_->call_, &pending_call_->deadline_,
        &pending_call_->request_metadata_, nullptr, cq_, cq_,
        this);
    }


    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);
  }

  grpc_server* srv_;
  void* reg_;
  grpc_completion_queue* cq_;
  CbT cb_;

  handler_type* pending_call_ = nullptr;
};
}  // namespace detail
}  // namespace server
}  // namespace easy_grpc
#endif