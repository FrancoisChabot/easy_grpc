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

#ifndef EASY_GRPC_CONFIG_INCLUDED_H
#define EASY_GRPC_CONFIG_INCLUDED_H

#include "var_future/future.h"
#include "var_future/stream_future.h"

namespace easy_grpc {
  using aom::expected;
  using aom::unexpected;
  using aom::Future;
  using aom::Stream_future;
  using aom::Promise;
  using aom::Stream_promise;
  using aom::is_future;
  using aom::is_future_v;
}


#include <cassert>
#define SGRPC_ASSERT(precond) assert(precond)

namespace easy_grpc {
// This enables agressive runtime validation.
constexpr bool easy_grpc_validation_enabled = true;
constexpr bool easy_grpc_tracing_enabled = false;
}  // namespace easy_grpc


#define EASY_GRPC_TRACE(ctx, location)
//#define EASY_GRPC_TRACE(ctx, location) std::cerr << #ctx << " : " << #location
//<< "\n";

#endif
