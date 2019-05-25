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

#ifndef EASY_GRPC_CLIENT_UNSECURE_CHANNEL_INCLUDED_H
#define EASY_GRPC_CLIENT_UNSECURE_CHANNEL_INCLUDED_H

#include "easy_grpc/client/channel.h"

namespace easy_grpc {
namespace client {
class Unsecure_channel : public Channel {
 public:
  Unsecure_channel(const std::string& addr, Completion_queue* default_pool);
};
}  // namespace client
}  // namespace easy_grpc
#endif