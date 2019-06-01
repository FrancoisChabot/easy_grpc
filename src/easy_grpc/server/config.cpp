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

#include "easy_grpc/server/server.h"
#include "easy_grpc/server/service.h"
#include "easy_grpc/server/service_impl.h"

#include <cassert>
#include <set>

namespace easy_grpc {

namespace server {

Config& Config::with_default_listening_queues(Completion_queue_set queues) {
  default_queues_ = std::move(queues);
  return *this;
}

Config& Config::with_service(Service_config cfg) {
  service_cfgs_.push_back(std::move(cfg));
  return *this;
}

Config& Config::with_listening_port(std::string addr,
                                    std::shared_ptr<Credentials> creds,
                                    int* bound_port) {
  ports_.push_back({std::move(addr), creds, bound_port});
  return *this;
}

}  // namespace server

}  // namespace easy_grpc