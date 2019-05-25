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

namespace easy_grpc {

namespace server {
Config& Config::with_service(Service* service) {
  services_.push_back(service);
  return *this;
}

Config& Config::with_listening_port(std::string addr,
                                    std::shared_ptr<Credentials> creds) {
  ports_.push_back({std::move(addr), creds});
  return *this;
}

Server::Server(Config cfg) { impl_ = grpc_server_create(nullptr, nullptr); }

Server::~Server() { cleanup_(); }

Server::Server(Server&& rhs) : impl_(rhs.impl_) { rhs.impl_ = nullptr; }

Server& Server::operator=(Server&& rhs) {
  cleanup_();
  impl_ = rhs.impl_;
  rhs.impl_ = nullptr;

  return *this;
}

void Server::cleanup_() {
  if (impl_) {
    grpc_server_destroy(impl_);
  }
}
}  // namespace server

}  // namespace easy_grpc