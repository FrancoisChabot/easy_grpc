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

#ifndef EASY_GRPC_SERVER_SERVICE_CONFIG_H_INCLUDED
#define EASY_GRPC_SERVER_SERVICE_CONFIG_H_INCLUDED

#include "easy_grpc/server/service_impl.h"
#include "easy_grpc/function_traits.h"
#include "var_future/future.h"

#include <string>

namespace easy_grpc {

namespace server {

class Service_config {
 public:
  Service_config(const char* name) : name_(name) {}

  template <typename CbT>
  void add_method(const char* name, CbT cb, Completion_queue_set={}) {
    using cb_traits = function_traits<CbT>;
    
    constexpr bool c_streaming = is_server_reader_v<typename cb_traits::template arg<0>::type>;
    // This is the check we WANT, but it's not that simple...
    // constexpr bool s_streaming = cb_traits::arity > 1 && is_server_writer_v<typename cb_traits::template arg<1>::type>;
    constexpr bool s_streaming = cb_traits::arity > 1;

    if constexpr(c_streaming && s_streaming) {

    }
    else if constexpr(c_streaming) {
      methods_.emplace_back(detail::make_client_streaming_method(name, std::move(cb)));
    }
    else if constexpr(s_streaming) {
      methods_.emplace_back(detail::make_server_streaming_method(name, std::move(cb)));
    }
    else {
      methods_.emplace_back(detail::make_unary_method(name, std::move(cb)));
    }

  }

  const std::vector<std::unique_ptr<detail::Method>>& methods() const {
    return methods_;
  }

 private:
  const char* name_;
  std::vector<std::unique_ptr<detail::Method>> methods_;
};
}  // namespace server

}  // namespace easy_grpc

#endif
