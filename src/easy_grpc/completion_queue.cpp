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

#include "easy_grpc/completion_queue.h"
#include "easy_grpc/config.h"

#include <cassert>

namespace easy_grpc {
Completion_queue::Completion_queue()
    : handle_(grpc_completion_queue_create_for_next(nullptr)) {
  thread_ = std::thread([this]() { worker_main(); });
}

Completion_queue::~Completion_queue() {
  grpc_completion_queue_shutdown(handle_);
  thread_.join();
  grpc_completion_queue_destroy(handle_);
}

void Completion_queue::worker_main() {
  //EASY_GRPC_TRACE(Completion_queue, start);

  while (1) {
    auto event = grpc_completion_queue_next(
        handle_, gpr_inf_future(GPR_CLOCK_REALTIME), nullptr);
    if (event.type == GRPC_OP_COMPLETE) {
      Completion* completion = reinterpret_cast<Completion*>(event.tag);

      static_assert(noexcept(completion->exec(event.success)));
      bool kill = completion->exec(event.success);
      if(kill) {
        delete completion;
      }
    } else {
      assert(event.type == GRPC_QUEUE_SHUTDOWN);
      break;
    }
  }
}
}  // namespace easy_grpc