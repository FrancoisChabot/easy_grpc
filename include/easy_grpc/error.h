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

#ifndef EASY_GRPC_ERROR_H_INCLUDED
#define EASY_GRPC_ERROR_H_INCLUDED

#include "grpc/grpc.h"

#include <stdexcept>

namespace easy_grpc {

  class Rpc_error : public std::runtime_error {
  public:
    Rpc_error(grpc_status_code code, const char* what) 
      : std::runtime_error(what)
      , code_(code) {}
      
  private:
    grpc_status_code code_;
  };

  namespace error {
    Rpc_error invalid_argument(const char* what) {
      return Rpc_error(GRPC_STATUS_INVALID_ARGUMENT, what);
    }
  }

}

#endif