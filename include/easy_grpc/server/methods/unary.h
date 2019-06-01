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

#ifndef EASY_GRPC_SERVER_METHOD_UNARY_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_UNARY_H_INCLUDED

#include "easy_grpc/config.h"
#include "easy_grpc/server/methods/method.h"

#include "easy_grpc/server/methods/unary_handler.h"
#include "easy_grpc/third_party/function_traits.h"

#include <cassert>

namespace easy_grpc {
namespace server {
namespace detail {

template <typename CbT, typename HandlerT>
class Unary_call_listener : public Completion_queue::Completion {
 public:
  Unary_call_listener(grpc_server* server, void* registration,
                      grpc_completion_queue* cq, CbT cb)
      : srv_(server), reg_(registration), cq_(cq), cb_(std::move(cb)) {
    // It's really important that inject is not called here. As the object
    // could end up being deleted before it's fully constructed.
  }

  ~Unary_call_listener() {
    if (pending_call_) {
      delete pending_call_;
    }
  }

  bool exec(bool success) noexcept override {
    EASY_GRPC_TRACE(Unary_call_listener, exec);

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
    EASY_GRPC_TRACE(Unary_call_listener, inject);

    assert(pending_call_ == nullptr);

    pending_call_ = new HandlerT;
    auto status = grpc_server_request_registered_call(
        srv_, reg_, &pending_call_->call_, &pending_call_->deadline_,
        &pending_call_->request_metadata_, &pending_call_->payload_, cq_, cq_,
        this);
  }

  CbT cb_;
  grpc_server* srv_;
  void* reg_;
  grpc_completion_queue* cq_;

  HandlerT* pending_call_ = nullptr;
};

template <typename CbT>
class Unary_method : public Method {
 public:
  Unary_method(const char* name, CbT cb) : Method(name), cb_(cb) {}

  void listen(grpc_server* server, void* registration,
              grpc_completion_queue* cq) override {
    using InT = typename function_traits<CbT>::template arg<0>::type;
    using OutT = typename function_traits<CbT>::result_type;

    using HandlerT = std::conditional_t<is_future_v<OutT>,
                                        Unary_async_call_handler<InT, OutT>,
                                        Unary_sync_call_handler<InT, OutT> >;

    auto handler =
        new Unary_call_listener<CbT, HandlerT>(server, registration, cq, cb_);
    handler->inject();
  }

 private:
  CbT cb_;
};

}  // namespace detail
}  // namespace server
}  // namespace easy_grpc
#endif