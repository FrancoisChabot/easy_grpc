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

#ifndef EASY_GRPC_CLIENT_METHOD_STUB_INCLUDED_H
#define EASY_GRPC_CLIENT_METHOD_STUB_INCLUDED_H

#include "grpc/grpc.h"

#include "easy_grpc/client/stub_impl.h"

#include <string>
#include <vector>

namespace easy_grpc {

namespace client {

template<typename InT, typename OutT>
class Method_stub {
public:
  Method_stub(const char * method_name, Channel* channel,Completion_queue* q = nullptr)
    : channel_(channel)
    , default_queue_(q?q:channel->default_queue())
    , tag_(channel->register_method(method_name)) {}

  Future<OutT> operator()(InT req, Call_options options={}) {
    if(!options.completion_queue) { options.completion_queue = default_queue_; } ;
    return start_unary_call<OutT>(channel_, tag_, std::move(req), std::move(options));
  }

private:
  Channel* channel_;
  Completion_queue* default_queue_;
  void * tag_;
};

}  // namespace client
}  // namespace easy_grpc
#endif