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

#ifndef EASY_GRPC_SERVER_SERVER_H_INCLUDED
#define EASY_GRPC_SERVER_SERVER_H_INCLUDED

#include "easy_grpc/server/credentials.h"
#include "easy_grpc/completion_queue.h"

#include "grpc/grpc.h"

#include <memory>
#include <string>
#include <vector>

namespace easy_grpc {

class Completion_queue;
namespace server {

class Service;

class Config {
 public:
  Config& with_default_listening_queues(Completion_queue_set);

  Config& with_service(Service*);

  Config& with_listening_port(std::string addr,
                              std::shared_ptr<Credentials> creds = {});

 private:
  struct Bound_service {
    Service* service;
  };

  struct Port {
    std::string addr;
    std::shared_ptr<Credentials> creds;
  };

  Completion_queue_set default_queues_;
  std::vector<Bound_service> services_;
  std::vector<Port> ports_;

  friend class Server;
};

class Server {
  grpc_server* impl_ = nullptr;

 public:
  Server(const Config& cfg);
  Server(Server&& rhs);
  Server& operator=(Server&& rhs);
  ~Server();

  grpc_server* handle() {return impl_;}
  
 private:
  void add_listening_ports_(const Config& cfg);
  void cleanup_();

  // Noncopyable
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  Completion_queue_set default_queues_;
};
}  // namespace server

}  // namespace easy_grpc

#endif