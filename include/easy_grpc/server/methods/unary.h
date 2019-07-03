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

#ifndef EASY_GRPC_SERVER_METHOD_UNARY_HANDLER_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_UNARY_HANDLER_H_INCLUDED

#include "easy_grpc/config.h"
#include "easy_grpc/error.h"
#include "easy_grpc/serialize.h"
#include "easy_grpc/function_traits.h"
#include "easy_grpc/server/methods/ops.h"
#include "var_future/future.h"

#include "grpc/grpc.h"

#include <cassert>

namespace easy_grpc {
namespace server {
namespace detail {

// Unary calls are a bit special in the sense that we allow the handlers to be fully synchronous by returning
// a RepT (as opposed to a Future<RepT>)
template <typename RepT>
class Unary_call_handler_base : public Completion_callback {
 public:
  static constexpr bool immediate_payload = true;
  
  Unary_call_handler_base() {
    grpc_metadata_array_init(&request_metadata_);
    grpc_metadata_array_init(&server_metadata_);
  }

  ~Unary_call_handler_base() {
    if(payload_) {
      grpc_byte_buffer_destroy(payload_);
    }
  
    if(call_) {
      grpc_call_unref(call_);
    }
    grpc_metadata_array_destroy(&request_metadata_);
    grpc_metadata_array_destroy(&server_metadata_);
  }

  bool exec(bool, bool) noexcept override {
    return true;
  }

  void finish(expected<RepT> rep) {
    if (rep.has_value()) {
      send_unary_response(call_, rep.value(), &server_metadata_, &cancelled_, completion_tag());
    } else {
      // The call has failed.
      send_failure(call_, rep.error(), &server_metadata_, &cancelled_, completion_tag());
    }
  }

  grpc_call* call_ = nullptr;

  //Request-related
  gpr_timespec deadline_;
  grpc_metadata_array request_metadata_;
  grpc_byte_buffer* payload_ = nullptr;

  //Reply-related
  int cancelled_ = false;
  grpc_metadata_array server_metadata_;
};

template <typename ReqT, typename RepT, bool sync>
class Unary_call_handler;

// Synchronous
template <typename ReqT, typename RepT>
class Unary_call_handler<ReqT, RepT, true> : public Unary_call_handler_base<RepT> {
 public:
  template <typename HandlerT>
  void perform(const HandlerT& handler) {
    assert(this->payload_);
    auto req = deserialize<ReqT>(this->payload_);
    expected<RepT> result;
    try {
      result = handler(req);
    } catch (...) {
      result = unexpected{std::current_exception()};
    }

    this->finish(std::move(result));
  }
};

// Asynchronous
template <typename ReqT, typename RepT>
class Unary_call_handler<ReqT, RepT, false>
    : public Unary_call_handler_base<RepT> {
 public:
  using value_type = RepT;

  template <typename HandlerT>
  void perform(const HandlerT& handler) {
    assert(this->payload_);
    auto req = deserialize<ReqT>(this->payload_);
    
    try {
      handler(req).finally(
        [this](expected<value_type> rep) { this->finish(rep); });
    } catch (...) {
      this->finish(unexpected{std::current_exception()});
    }
  }
};
}  // namespace detail
}  // namespace server
}  // namespace easy_grpc
#endif
