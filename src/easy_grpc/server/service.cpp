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

#include "easy_grpc/server/service.h"

namespace easy_grpc {
  namespace server {
    void Service::set_default_queues(Completion_queue_set queues) {
      default_queues_ = std::move(queues);
    }

    const Completion_queue_set& Service::default_queues() {
      return default_queues_;
    }
  }
}