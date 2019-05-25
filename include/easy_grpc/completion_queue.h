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

#ifndef EASY_GRPC_COMPLETION_QUEUE_INCLUDED_H
#define EASY_GRPC_COMPLETION_QUEUE_INCLUDED_H

#include <thread>
#include "grpc/grpc.h"

namespace easy_grpc {
// A completion queue, with a matching thread that consumes from it.
class Completion_queue {
 public:
  class Completion {
   public:
    virtual ~Completion() {}
    virtual void exec(bool success) = 0;
  };

  Completion_queue();
  ~Completion_queue();

  grpc_completion_queue* handle() { return handle_; }

 private:
  void worker_main();
  std::thread thread_;
  grpc_completion_queue* handle_;
};
}  // namespace easy_grpc
#endif