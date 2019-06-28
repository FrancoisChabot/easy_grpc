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

#ifndef EASY_GRPC_SERVER_METHOD_SERVER_READER_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_SERVER_READER_H_INCLUDED

#include "easy_grpc/config.h"

#include <cassert>
#include <iostream>

namespace easy_grpc {


template<typename T>
class Server_reader_interface {
public:
  virtual ~Server_reader_interface() {};

  virtual Future<void> for_each(std::function<void(T)>) = 0;
};

template<typename T>
struct Server_reader {
  using value_type = T;

  Server_reader(Server_reader_interface<T>* tgt) : tgt_(tgt) {}
  
  template<typename CbT>
  Future<void> for_each(CbT cb) {
    return tgt_->for_each(std::move(cb));
  }

  Server_reader_interface<T>* tgt_;
};
}

#endif
