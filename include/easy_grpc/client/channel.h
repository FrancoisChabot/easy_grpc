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

#ifndef EASY_GRPC_CLIENT_CHANNEL_INCLUDED_H
#define EASY_GRPC_CLIENT_CHANNEL_INCLUDED_H

#include "grpc/grpc.h"

#include <string>
#include <vector>

namespace easy_grpc {

class Completion_queue;

namespace client {
class Channel {
 public:
  Channel() = default;

  virtual ~Channel() {
    if (handle_) {
     grpc_channel_destroy(handle_);
    }
  }

  void* register_method(const char* name) {
    return grpc_channel_register_call(handle_, name, nullptr, nullptr); 
  }

  Completion_queue* default_queue() const { return default_queue_; }
  grpc_channel* handle() const { return handle_; }

 protected:
  Channel(grpc_channel* handle, Completion_queue* queue)
    : handle_(handle), default_queue_(queue) {}

  Channel(Channel&& rhs)     
    : handle_(rhs.handle_), default_queue_(rhs.default_queue_) {
    rhs.handle_ = nullptr;
    rhs.default_queue_ = nullptr;
  }

  Channel& operator=(Channel&& rhs) {
    handle_ = rhs.handle_;
    default_queue_ = rhs.default_queue_;

    rhs.handle_ = nullptr;
    rhs.default_queue_ = nullptr;
    return *this;
  }

 private:
  grpc_channel* handle_ = nullptr;
  Completion_queue* default_queue_ = nullptr;

  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;
};
}  // namespace client
}  // namespace easy_grpc
#endif