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

#include "easy_grpc/client/unsecure_channel.h"

namespace easy_grpc {
namespace client {
Unsecure_channel::Unsecure_channel(const std::string& addr,
                                   Completion_queue* default_pool)
    : Channel(grpc_insecure_channel_create(addr.c_str(), nullptr, nullptr),
              default_pool) {}
}  // namespace client
}  // namespace easy_grpc
