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
      : std::runtime_error(what), code_(code) {}

  grpc_status_code code() const { return code_; }

 private:
  grpc_status_code code_;
};

namespace error {
inline Rpc_error cancelled(const char* what) {
  return Rpc_error(GRPC_STATUS_CANCELLED, what);
}

inline Rpc_error unknown(const char* what) {
  return Rpc_error(GRPC_STATUS_UNKNOWN, what);
}

inline Rpc_error invalid_argument(const char* what) {
  return Rpc_error(GRPC_STATUS_INVALID_ARGUMENT, what);
}

inline Rpc_error deadline_exceeded(const char* what) {
  return Rpc_error(GRPC_STATUS_DEADLINE_EXCEEDED, what);
}

inline Rpc_error not_found(const char* what) {
  return Rpc_error(GRPC_STATUS_NOT_FOUND, what);
}

inline Rpc_error already_exists(const char* what) {
  return Rpc_error(GRPC_STATUS_ALREADY_EXISTS, what);
}

inline Rpc_error permission_denied(const char* what) {
  return Rpc_error(GRPC_STATUS_PERMISSION_DENIED, what);
}

inline Rpc_error unauthenticated(const char* what) {
  return Rpc_error(GRPC_STATUS_UNAUTHENTICATED, what);
}

inline Rpc_error resource_exhausted(const char* what) {
  return Rpc_error(GRPC_STATUS_RESOURCE_EXHAUSTED, what);
}

inline Rpc_error failed_precondition(const char* what) {
  return Rpc_error(GRPC_STATUS_FAILED_PRECONDITION, what);
}

inline Rpc_error aborted(const char* what) {
  return Rpc_error(GRPC_STATUS_ABORTED, what);
}

inline Rpc_error out_of_range(const char* what) {
  return Rpc_error(GRPC_STATUS_OUT_OF_RANGE, what);
}

inline Rpc_error unimplemented(const char* what) {
  return Rpc_error(GRPC_STATUS_UNIMPLEMENTED, what);
}

inline Rpc_error internal(const char* what) {
  return Rpc_error(GRPC_STATUS_INTERNAL, what);
}

inline Rpc_error unavailable(const char* what) {
  return Rpc_error(GRPC_STATUS_UNAVAILABLE, what);
}

inline Rpc_error data_loss(const char* what) {
  return Rpc_error(GRPC_STATUS_DATA_LOSS, what);
}
}  // namespace error

}  // namespace easy_grpc

#endif