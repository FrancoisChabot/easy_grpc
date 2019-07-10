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
#include "easy_grpc/server/methods/call_handler.h"
#include "var_future/future.h"

#include "grpc/grpc.h"

#include <cassert>

namespace easy_grpc {
namespace server {
namespace detail {

// Unary calls are a bit special in the sense that we allow the handlers to be fully synchronous by returning
// a RepT (as opposed to a Future<RepT>)
template <typename RepT>
class Unary_call_handler_base : public Call_handler {
public:
  grpc_byte_buffer* payload_ = nullptr;
  static constexpr bool immediate_payload = true;
  
  Unary_call_handler_base() {}

  ~Unary_call_handler_base() {
    if(payload_) {
      grpc_byte_buffer_destroy(payload_);
    }
  }

  bool exec(bool, std::bitset<4>) noexcept override {
    return true;
  }

  void finish(expected<RepT> rep) {
    if (rep.has_value()) {
      send_unary_response(rep.value(), true, false);
    } else {
      send_failure(rep.error(), true, false);
    }
  }
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
