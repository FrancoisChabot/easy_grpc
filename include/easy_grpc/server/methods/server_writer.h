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

#ifndef EASY_GRPC_SERVER_METHOD_SERVER_WRITER_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_SERVER_WRITER_H_INCLUDED

#include "easy_grpc/config.h"

#include <cassert>
#include <iostream>

namespace easy_grpc {


template<typename T>
class Server_writer_interface {
public:
  virtual ~Server_writer_interface() {};

  virtual void push(const T&) = 0;
  virtual void finish() = 0;
  virtual void fail(std::exception_ptr) = 0;
};



template<typename T>
struct Server_writer {
  using value_type = T;

  Server_writer(Server_writer_interface<T>* tgt) : tgt_(tgt) {}

  void push(const T& v) {
    tgt_->push(v);
  }

  void finish() {
    tgt_->finish();
  }

  template<typename ErrT>
  void fail(ErrT&& err) {
    tgt_->fail(std::make_exception_ptr(std::forward<ErrT>(err)));
  }

  Server_writer_interface<T>* tgt_;
};

}

#endif