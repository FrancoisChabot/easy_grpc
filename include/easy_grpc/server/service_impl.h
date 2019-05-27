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

#ifndef EASY_GRPC_SERVER_SERVICE_IMPL_INCLUDED_H
#define EASY_GRPC_SERVER_SERVICE_IMPL_INCLUDED_H

#include "easy_grpc/completion_queue.h"
#include <iostream>

namespace easy_grpc {

namespace server {
  class Server;
  namespace detail {

    class Unary_call_handler : public Completion_queue::Completion {
    public:
      void exec(bool success) override {
        std::cerr << "ping\n" ;
      }

      grpc_call* call_;
      gpr_timespec deadline_;
      grpc_metadata_array request_metadata_;
      grpc_byte_buffer* payload_;
    };

    class Method {
    public:
      Method(const char* name) : name_(name) {}
      virtual ~Method() {}

      const char* name() const { return name_;}

      void set_queues(Completion_queue_set queues) {queues_ = queues;}
      const Completion_queue_set& queues() const {return queues_;}


      virtual void listen(grpc_server* server, void* registration, grpc_completion_queue* cq) {
        auto handler = new Unary_call_handler();

        auto status = grpc_server_request_registered_call(
          server, registration, &handler->call_,
          &handler->deadline_, &handler->request_metadata_,
          &handler->payload_, cq, cq, handler);
      }

    private:
      Completion_queue_set queues_;
      const char* name_;
    };

    class Unary_method : public Method {
    public:
      Unary_method(const char * name) : Method(name) {}
    };

    class Method_visitor {
      public:
        virtual ~Method_visitor() {}
        virtual void visit(Unary_method&) = 0;
    };
  }
}

}  // namespace easy_grpc
#endif