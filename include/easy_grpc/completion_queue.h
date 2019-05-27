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

#include "grpc/grpc.h"

#include <thread>
#include <vector>

namespace easy_grpc {
// A completion queue, with a matching thread that consumes from it.
class Completion_queue {
 public:
  class Completion {
   public:
    virtual ~Completion() {}

    // TODO: replace bool with an enum
    virtual bool exec(bool success) = 0;
  };

  Completion_queue();
  ~Completion_queue();

  grpc_completion_queue* handle() { return handle_; }

 private:
  void worker_main();
  std::thread thread_;
  grpc_completion_queue* handle_;
};

// Each server-side method is bound to a set of completion queues.
class Completion_queue_set {
public:
  Completion_queue_set() = default;
  Completion_queue_set(Completion_queue_set&&) = default;
  Completion_queue_set(const Completion_queue_set&) = default;
  Completion_queue_set& operator=(Completion_queue_set&&) = default;
  Completion_queue_set& operator=(const Completion_queue_set&) = default;

  template<typename IteT>
  Completion_queue_set(IteT begin, IteT end) {
    queues_.reserve(std::distance(begin, end));
    for(; begin != end; ++begin) {
      queues_.push_back(std::ref(*begin));
    }
  }

  auto begin() const {
    return queues_.begin();
  }

  auto end() const {
    return queues_.end();
  }

  bool empty() const {return queues_.empty();}

private:
  std::vector<std::reference_wrapper<Completion_queue>> queues_;
};
}  // namespace easy_grpc
#endif