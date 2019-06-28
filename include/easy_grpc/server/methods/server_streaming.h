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

#ifndef EASY_GRPC_SERVER_METHOD_SERVER_STREAMING_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_SERVER_STREAMING_H_INCLUDED

#include "easy_grpc/config.h"
#include "easy_grpc/server/methods/method.h"

#include "easy_grpc/function_traits.h"
#include "easy_grpc/serialize.h"

#include <cassert>
#include <queue>

namespace easy_grpc {
namespace server {
namespace detail {

template <typename ReqT, typename RepT>
class Server_streaming_call_handler : public Completion_queue::Completion, public Server_writer_interface<RepT> {
public:
  template<typename CbT>
  void perform(const CbT& handler) {
    assert(this->payload_);
    
    auto req = deserialize<ReqT>(this->payload_);
    grpc_byte_buffer_destroy(this->payload_);

    batch_in_flight_ = true;
    std::array<grpc_op, 1> ops;

    ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
    ops[0].flags = 0;
    ops[0].reserved = 0;
    ops[0].data.send_initial_metadata.count = server_metadata_.count;
    ops[0].data.send_initial_metadata.metadata = server_metadata_.metadata;
    ops[0].data.send_initial_metadata.maybe_compression_level.is_set = false;

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), static_cast<Completion_queue::Completion*>(this), nullptr);


    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);

    try {
      handler(req, this);
    } catch(...) {
    }
  }

  void push(const RepT& val) override {
    std::lock_guard l(mtx_);

    auto buffer = serialize(val);
    
    grpc_op op;
    op.op = GRPC_OP_SEND_MESSAGE;
    op.flags = 0;
    op.reserved = nullptr;
    op.data.send_message.send_message = buffer;

    pending_ops_.push(op);

    flush_();
  }

  void finish() override {
    std::lock_guard l(mtx_);

    finished_ = true;

    if(!batch_in_flight_) {
      send_end();
    }
  }

  void fail(std::exception_ptr) override {
    std::lock_guard l(mtx_);
  }

  bool exec(bool) noexcept override {
    std::lock_guard l(mtx_);
    batch_in_flight_ = false;

    if(!pending_ops_.empty()) {
      flush_();
      return false;
    }

    if(finished_) {
      if(!end_sent_) {
        send_end();
      }
      else {
        return true;
      }
    }
    return false;
  }

  void send_end() {
    std::array<grpc_op, 2> ops;

    ops[0].op = GRPC_OP_SEND_STATUS_FROM_SERVER;
    ops[0].flags = 0;
    ops[0].reserved = nullptr;
    ops[0].data.send_status_from_server.trailing_metadata_count = 0;
    ops[0].data.send_status_from_server.trailing_metadata = nullptr;
    ops[0].data.send_status_from_server.status = GRPC_STATUS_OK;
    ops[0].data.send_status_from_server.status_details = nullptr;

    ops[1].op = GRPC_OP_RECV_CLOSE_ON_SERVER;
    ops[1].flags = 0;
    ops[1].reserved = nullptr;
    ops[1].data.recv_close_on_server.cancelled = &cancelled_;

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), static_cast<Completion_queue::Completion*>(this), nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);

    end_sent_ = true;
  }

  void flush_() {
    if(batch_in_flight_) {
      return ;
    }

    batch_in_flight_ = true;

    auto op = pending_ops_.front();
    pending_ops_.pop();

    assert(op.op == GRPC_OP_SEND_MESSAGE);
    
    auto status =
      grpc_call_start_batch(call_, &op, 1, static_cast<Completion_queue::Completion*>(this), nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);
    
    grpc_byte_buffer_destroy(op.data.send_message.send_message);
  }

  grpc_byte_buffer* payload_ = nullptr;
  int cancelled_ = false;
  grpc_call* call_ = nullptr;
  gpr_timespec deadline_;
  grpc_metadata_array request_metadata_;
  grpc_metadata_array server_metadata_;

  // I really wish we could do this without a mutex, somehow...
  std::mutex mtx_;
  bool end_sent_ = false;
  bool finished_ = false;
  bool batch_in_flight_ = false;
  std::queue<grpc_op> pending_ops_;
};


  
template <typename CbT>
class Server_streaming_call_listener : public Completion_queue::Completion {
    using InT = typename function_traits<CbT>::template arg<0>::type;
    using OutWriterT = typename function_traits<CbT>::template arg<1>::type;
    using OutT = typename OutWriterT::value_type;

    using handler_type = Server_streaming_call_handler<InT, OutT>;
 public:
  Server_streaming_call_listener(grpc_server* server, void* registration,
                      grpc_completion_queue* cq, CbT cb)
      : srv_(server), reg_(registration), cq_(cq), cb_(std::move(cb)) {
    // It's really important that inject is not called here. As the object
    // could end up being deleted before it's fully constructed.
  }

  ~Server_streaming_call_listener() {
    if (pending_call_) {
      delete pending_call_;
    }
  }

  bool exec(bool success) noexcept override {
    EASY_GRPC_TRACE(Server_streaming_call_listener, exec);

    if (success) {
      pending_call_->perform(cb_);
      pending_call_ = nullptr;

      // Listen for a new call.
      inject();
      return false;  // This object is recycled.
    }

    return true;
  }

  void inject() {
    EASY_GRPC_TRACE(Server_streaming_call_listener, inject);

    assert(pending_call_ == nullptr);


    pending_call_ = new handler_type;
    auto status = grpc_server_request_registered_call(
        srv_, reg_, &pending_call_->call_, &pending_call_->deadline_,
        &pending_call_->request_metadata_, &pending_call_->payload_, cq_, cq_,
        this);

    assert(status == GRPC_CALL_OK);
  }

  
  grpc_server* srv_;
  void* reg_;
  grpc_completion_queue* cq_;
  CbT cb_;

  handler_type* pending_call_ = nullptr;
};

template <typename CbT>
class Server_streaming_method : public Method {
 public:
  Server_streaming_method(const char* name, CbT cb) : Method(name), cb_(cb) {}

  void listen(grpc_server* server, void* registration,
              grpc_completion_queue* cq) override {

    auto handler =
        new Server_streaming_call_listener<CbT>(server, registration, cq, cb_);
    handler->inject();
  }

  bool immediate_payload_read() const override {
    return true;
  }
 private:
  CbT cb_;
};
}
}
}
#endif
