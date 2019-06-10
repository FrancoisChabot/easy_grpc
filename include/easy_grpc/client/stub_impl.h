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

#ifndef EASY_GRPC_CLIENT_STUB_IMPL_INCLUDED_H
#define EASY_GRPC_CLIENT_STUB_IMPL_INCLUDED_H

#include "easy_grpc/completion_queue.h"
#include "easy_grpc/error.h"
#include "easy_grpc/serialize.h"
#include "easy_grpc/client/channel.h"

#include "grpc/grpc.h"
#include "grpc/support/alloc.h"

#include <cstring>
#include <iostream>
#include <queue>

namespace easy_grpc {

template<typename T>
struct Client_reader {
  struct iterator {
    T& operator*() { return *next_; }
    void operator++() {}
    bool operator!=(const iterator&) {return false;}
  
    T* next_= nullptr;
  };

  
  iterator begin() {return {}; }
  iterator end() { return {}; }
};

template<typename ReqT>
class Client_writer_interface {
public:
  virtual ~Client_writer_interface() {}
  virtual void client_add(const ReqT& val) = 0;
  virtual void client_finish() = 0;

};

template<typename T>
struct Client_writer {
  Client_writer(Client_writer_interface<T>* tgt) : tgt_(tgt) {}

  using value_type = T;

  void push(const T& val) {
    tgt_->client_add(val);
  }
  void finish() {
    tgt_->client_finish();
  }

  Client_writer_interface<T>* tgt_;
};

namespace client {
struct Call_options {
  Completion_queue* completion_queue = nullptr;
  gpr_timespec deadline = gpr_inf_future(GPR_CLOCK_REALTIME);
};

//*********************************************************************************//
namespace detail {
template <typename RepT>
class Unary_call_completion final : public Completion_queue::Completion {
 public:
  Unary_call_completion(grpc_call* call) : call_(call) {
    grpc_metadata_array_init(&trailing_metadata_);
    grpc_metadata_array_init(&server_metadata_);
  }

  ~Unary_call_completion() {
    grpc_metadata_array_destroy(&server_metadata_);
    grpc_metadata_array_destroy(&trailing_metadata_);

    if (recv_buffer_) {
      grpc_byte_buffer_destroy(recv_buffer_);
    }

    grpc_call_unref(call_);
  }

  void fail() {
    try {
      throw error::internal("failed to start call");
    } catch (...) {
      rep_.set_exception(std::current_exception());
    }
  }

  bool exec(bool) noexcept override {
    if (status_ == GRPC_STATUS_OK) {
      rep_.set_value(deserialize<RepT>(recv_buffer_));
    } else {
      try {
        auto str = grpc_slice_to_c_string(status_details_);
        auto err = Rpc_error(status_, str);
        gpr_free(str);
        throw err;
      } catch (...) {
        rep_.set_exception(std::current_exception());
      }
    }
    return true;
  }

  grpc_call* call_;
  grpc_metadata_array server_metadata_;
  Promise<RepT> rep_;
  grpc_byte_buffer* recv_buffer_;

  grpc_metadata_array trailing_metadata_;
  grpc_status_code status_;
  grpc_slice status_details_;
  const char* error_string_;
};
}  // namespace detail


template <typename RepT, typename ReqT>
Future<RepT> start_unary_call(Channel* channel, void* tag, const ReqT& req,
                              Call_options options) {
  assert(options.completion_queue);

  auto call = grpc_channel_create_registered_call(
      channel->handle(), nullptr, GRPC_PROPAGATE_DEFAULTS,
      options.completion_queue->handle(), tag, options.deadline, nullptr);
  auto completion = new detail::Unary_call_completion<RepT>(call);
  auto buffer = serialize(req);

  std::array<grpc_op, 6> ops;

  ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
  ops[0].flags = 0;
  ops[0].reserved = nullptr;
  ops[0].data.send_initial_metadata.count = 0;
  ops[0].data.send_initial_metadata.maybe_compression_level.is_set = 0;

  ops[1].op = GRPC_OP_SEND_MESSAGE;
  ops[1].flags = 0;
  ops[1].reserved = nullptr;
  ops[1].data.send_message.send_message = buffer;

  ops[2].op = GRPC_OP_RECV_INITIAL_METADATA;
  ops[2].flags = 0;
  ops[2].reserved = 0;
  ops[2].data.recv_initial_metadata.recv_initial_metadata =
      &completion->server_metadata_;

  ops[3].op = GRPC_OP_RECV_MESSAGE;
  ops[3].flags = 0;
  ops[3].reserved = 0;
  ops[3].data.recv_message.recv_message = &completion->recv_buffer_;

  ops[4].op = GRPC_OP_SEND_CLOSE_FROM_CLIENT;
  ops[4].flags = 0;
  ops[4].reserved = 0;

  ops[5].op = GRPC_OP_RECV_STATUS_ON_CLIENT;
  ops[5].flags = 0;
  ops[5].reserved = 0;
  ops[5].data.recv_status_on_client.trailing_metadata =
      &completion->trailing_metadata_;
  ops[5].data.recv_status_on_client.status = &completion->status_;
  ops[5].data.recv_status_on_client.status_details =
      &completion->status_details_;
  ops[5].data.recv_status_on_client.error_string = &completion->error_string_;

  auto result = completion->rep_.get_future();
  auto status =
      grpc_call_start_batch(call, ops.data(), ops.size(), completion, nullptr);

  if (status != GRPC_CALL_OK) {
    completion->fail();
    delete completion;
  }

  grpc_byte_buffer_destroy(buffer);

  return result;
}

//*********************************************************************************//

template <typename RepT, typename ReqT>
Client_reader<RepT> start_server_streaming_call(Channel*, void*, const ReqT&, Call_options) {
  return {};
}


//*********************************************************************************//

// This is not quite as nice as it could be...
//
template<typename RepT, typename ReqT>
class Client_streaming_call_session final 
  : public Completion_queue::Completion
  , public Client_writer_interface<ReqT> {
public:
  Client_streaming_call_session(grpc_call* call) 
    : call_(call) {
    grpc_metadata_array_init(&trailing_metadata_);
    grpc_metadata_array_init(&server_metadata_);

    std::array<grpc_op, 2> pending_ops;
    
    pending_ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
    pending_ops[0].flags = 0;
    pending_ops[0].reserved = nullptr;
    pending_ops[0].data.send_initial_metadata.count = 0;
    pending_ops[0].data.send_initial_metadata.maybe_compression_level.is_set = 0;

    pending_ops[1].op = GRPC_OP_RECV_INITIAL_METADATA;
    pending_ops[1].flags = 0;
    pending_ops[1].reserved = 0;
    pending_ops[1].data.recv_initial_metadata.recv_initial_metadata = &server_metadata_;

    grpc_call_start_batch(call_, pending_ops.data(), pending_ops.size(), static_cast<Completion_queue::Completion*>(this), nullptr);
    batch_in_flight_ = true;
  }

  ~Client_streaming_call_session() {
    assert(pending_ops_.empty());

    grpc_metadata_array_destroy(&server_metadata_);
    grpc_metadata_array_destroy(&trailing_metadata_);

    if (recv_buffer_) {
      grpc_byte_buffer_destroy(recv_buffer_);
    }

    grpc_call_unref(call_);
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

  void client_add(const ReqT& val) override {
    std::lock_guard l(mtx_);

    auto buffer = serialize(val);
    
    grpc_op op;
    op.op = GRPC_OP_SEND_MESSAGE;
    op.flags = 0;
    op.reserved = nullptr;
    op.data.send_message.send_message = buffer;

    pending_ops_.push(op);

    // Send whatever we have.
    flush_();
  }

  void send_end() {

    std::array<grpc_op, 3> ops;

    ops[0].op = GRPC_OP_SEND_CLOSE_FROM_CLIENT;
    ops[0].flags = 0;
    ops[0].reserved = 0;

    ops[1].op = GRPC_OP_RECV_MESSAGE;
    ops[1].flags = 0;
    ops[1].reserved = 0;
    ops[1].data.recv_message.recv_message = &recv_buffer_;

    ops[2].op = GRPC_OP_RECV_STATUS_ON_CLIENT;
    ops[2].flags = 0;
    ops[2].reserved = 0;
    ops[2].data.recv_status_on_client.trailing_metadata =
        &trailing_metadata_;
    ops[2].data.recv_status_on_client.status = &status_;
    ops[2].data.recv_status_on_client.status_details =
        &status_details_;
    ops[2].data.recv_status_on_client.error_string = &error_string_;

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), static_cast<Completion_queue::Completion*>(this), nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);

    end_sent_ = true;

  }
  void client_finish() {
    std::lock_guard l(mtx_);
    finished_ = true;
    
    if(!batch_in_flight_) {
      send_end();
    }
  }

  bool exec(bool) noexcept override {
    std::lock_guard l(mtx_);
    batch_in_flight_ = false;

    if(!pending_ops_.empty()) {
      flush_();
      return false;
    }

    if(finished_) {
      if(end_sent_) {
        if (status_ == GRPC_STATUS_OK) {
          rep_.set_value(deserialize<RepT>(recv_buffer_));
        } else {
        try {
          auto str = grpc_slice_to_c_string(status_details_);
          auto err = Rpc_error(status_, str);
          gpr_free(str);
          throw err;
        } catch (...) {
          rep_.set_exception(std::current_exception());
        }
      
      }

        return true;
      }
      send_end();
    }

    return false;
  }

  // I really wish we could do this without a mutex, somehow...
  std::mutex mtx_;
  bool batch_in_flight_ = false;
  bool finished_ = false;
  bool end_sent_ = false;

  grpc_call* call_;
  grpc_metadata_array server_metadata_;
  Promise<RepT> rep_;
  grpc_byte_buffer* recv_buffer_;

  grpc_metadata_array trailing_metadata_;
  grpc_status_code status_;
  grpc_slice status_details_;
  const char* error_string_;

  std::queue<grpc_op> pending_ops_;
};

template <typename RepT, typename ReqT>
std::tuple<Client_writer<ReqT>, Future<RepT>> start_client_streaming_call(Channel* channel, void* tag, Call_options options) {
  assert(options.completion_queue);

  auto call = grpc_channel_create_registered_call(
      channel->handle(), nullptr, GRPC_PROPAGATE_DEFAULTS,
      options.completion_queue->handle(), tag, options.deadline, nullptr);

  auto call_session = new Client_streaming_call_session<RepT, ReqT>(call);  

  return {Client_writer<ReqT>(call_session), call_session->rep_.get_future()};
}

//*********************************************************************************//

template <typename RepT, typename ReqT>
Client_reader<RepT> start_bidir_streaming_call(Channel* channel, void* tag, const ReqT& req, Call_options options) {
  return {};
}

}  // namespace client

}  // namespace easy_grpc
#endif
