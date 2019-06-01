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

#ifndef EASY_GRPC_ENVIRONMENT_H_INCLUDED
#define EASY_GRPC_ENVIRONMENT_H_INCLUDED

#include "grpc/grpc.h"

#include "easy_grpc/config.h"

#include <cassert>

namespace easy_grpc {

class [[nodiscard]] Environment {
  Environment(const Environment&) = delete;
  Environment& operator=(const Environment&) = delete;
  Environment(Environment &&) = delete;
  Environment& operator=(Environment&&) = delete;

 public:
  Environment();
  ~Environment();

  static void assert_valid() {
    if constexpr (easy_grpc_validation_enabled) {
      assert(is_valid());
    }
  }

  static bool is_valid();
};
}  // namespace easy_grpc

#endif