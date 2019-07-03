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

#ifndef EASY_GRPC_SERVER_METHOD_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_H_INCLUDED

#include "easy_grpc/completion_queue.h"
#include "easy_grpc/function_traits.h"

#include "easy_grpc/server/methods/listener.h"

namespace easy_grpc {
namespace server {
namespace detail {

template<typename T>
struct Arg_extractor {
  using type = T;
  static constexpr bool sync = true;
};

template<typename T>
struct Arg_extractor<Future<T>> {
  using type = T;
  static constexpr bool sync = false;
};

template<typename T>
struct Arg_extractor<Stream_future<T>> {
  using type = T;
  static constexpr bool sync = false;
};

class Method {
 public:
  Method(const char* name) : name_(name) {}
  virtual ~Method() {}

  const char* name() const { return name_; }

  void set_queues(Completion_queue_set queues) { queues_ = queues; }
  const Completion_queue_set& queues() const { return queues_; }

  virtual void listen(grpc_server* server, void* registration,
                      grpc_completion_queue* cq) = 0;

  virtual bool immediate_payload_read() const = 0;
 private:
  Completion_queue_set queues_;
  const char* name_;
};


template<template<typename, typename, bool> typename HandlerT, typename CbT>
class Method_impl : public Method {
  using CbArgT = typename function_traits<CbT>::template arg<0>::type;
  using CbResultT = typename function_traits<CbT>::result_type;

  using InT = typename Arg_extractor<CbArgT>::type;
  using OutT = typename Arg_extractor<CbResultT>::type;

  using handler_type = HandlerT<InT, OutT, Arg_extractor<CbResultT>::sync>;
public:
 Method_impl(const char* name, CbT cb) : Method(name), cb_(cb) {}

  void listen(grpc_server* server, void* registration,
              grpc_completion_queue* cq) override {

    auto listener = new Method_listener<CbT, handler_type>(server, registration, cq, cb_);
    listener->inject();
  }

  bool immediate_payload_read() const override {
    return handler_type::immediate_payload;
  }

 private:
  CbT cb_;
};
}  // namespace detail
}  // namespace server
}  // namespace easy_grpc
#endif