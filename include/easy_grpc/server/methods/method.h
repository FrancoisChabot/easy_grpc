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

#ifndef EASY_GRPC_SERVER_METHOD_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_H_INCLUDED

#include "easy_grpc/completion_queue.h"

namespace easy_grpc {
namespace server {
namespace detail {
  
    class Method {
    public:
      Method(const char* name) : name_(name) {}
      virtual ~Method() {}

      const char* name() const { return name_;}

      void set_queues(Completion_queue_set queues) {queues_ = queues;}
      const Completion_queue_set& queues() const {return queues_;}

      virtual void listen(grpc_server* server, void* registration, grpc_completion_queue* cq) = 0;

    private:
      Completion_queue_set queues_;
      const char* name_;
    };
}
}
}
#endif