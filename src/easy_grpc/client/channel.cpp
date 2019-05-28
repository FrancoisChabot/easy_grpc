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

#include "easy_grpc/client/channel.h"

namespace easy_grpc {
namespace client {
Channel::Channel(grpc_channel* handle, Completion_queue* queue)
    : handle_(handle), default_queue_(queue) {}

Channel::~Channel() {
  if (handle_) {
    grpc_channel_destroy(handle_);
  }
}

Channel::Channel(Channel&& rhs)
    : handle_(rhs.handle_), default_queue_(rhs.default_queue_) {
  rhs.handle_ = nullptr;
}

Completion_queue* Channel::default_queue() const { 
  return default_queue_; 
}

grpc_channel* Channel::handle() const { 
  return handle_; 
}

Channel& Channel::operator=(Channel&& rhs) {
  handle_ = rhs.handle_;
  default_queue_ = rhs.default_queue_;

  rhs.handle_ = nullptr;
  return *this;
}

void* Channel::register_method(const char* name) {
  return grpc_channel_register_call(handle_, name, nullptr, nullptr);
}
}  // namespace client
}  // namespace easy_grpc
