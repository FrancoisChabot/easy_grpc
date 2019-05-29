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

#ifndef EASY_GRPC_SERVER_SERVICE_H_INCLUDED
#define EASY_GRPC_SERVER_SERVICE_H_INCLUDED

#include "easy_grpc/server/service_impl.h"
#include "easy_grpc/completion_queue.h"

#include <string>

namespace easy_grpc {

namespace server {

class Service {
 public:
  virtual ~Service() {}

  void set_default_queues(Completion_queue_set queues);
  const Completion_queue_set& default_queues();
  
private:
  
  Completion_queue_set default_queues_;
  friend class Server;
};
}  // namespace server

}  // namespace easy_grpc

#endif