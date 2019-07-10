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

namespace client {
struct Call_options {
  Completion_queue* completion_queue = nullptr;
  gpr_timespec deadline = gpr_inf_future(GPR_CLOCK_REALTIME);
};

//*********************************************************************************//
namespace detail {
template <typename RepT>
class Unary_call_completion final : public Completion_callback {
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

  bool exec(bool, std::bitset<4>) noexcept override {
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

namespace detail {
template <typename RepT>
class Streaming_call_session final : public Completion_callback {
 public:
  Streaming_call_session(grpc_call* call) : call_(call) {
    grpc_metadata_array_init(&trailing_metadata_);
    grpc_metadata_array_init(&server_metadata_);
  }

  ~Streaming_call_session() {
    grpc_metadata_array_destroy(&server_metadata_);
    grpc_metadata_array_destroy(&trailing_metadata_);

    if (recv_buffer_) {
      grpc_byte_buffer_destroy(recv_buffer_);
    }

    grpc_call_unref(call_);
  }

  bool exec(bool, std::bitset<4>) noexcept override {
    bool all_done = finishing_;

    if(!all_done) {
      if( recv_buffer_) {
        auto data = recv_buffer_;
        recv_buffer_ = nullptr;

        std::array<grpc_op, 1> ops;

        ops[0].op = GRPC_OP_RECV_MESSAGE;
        ops[0].flags = 0;
        ops[0].reserved = 0;
        ops[0].data.recv_message.recv_message = &recv_buffer_;

        auto call_status =
            grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

        if (call_status != GRPC_CALL_OK) {
          assert(false);
        }

        reply_stream_promise_.push(deserialize<RepT>(data));
        grpc_byte_buffer_destroy(data);
      }
      else {
        std::array<grpc_op, 2> ops;

        ops[0].op = GRPC_OP_SEND_CLOSE_FROM_CLIENT;
        ops[0].flags = 0;
        ops[0].reserved = 0;

        ops[1].op = GRPC_OP_RECV_STATUS_ON_CLIENT;
        ops[1].flags = 0;
        ops[1].reserved = 0;
        ops[1].data.recv_status_on_client.trailing_metadata =
            &trailing_metadata_;
        ops[1].data.recv_status_on_client.status = &status_;
        ops[1].data.recv_status_on_client.status_details =
            &status_details_;
        ops[1].data.recv_status_on_client.error_string = &error_string_;
        
        finishing_ = true;

        auto call_status =
            grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

        if (call_status != GRPC_CALL_OK) {
          assert(false);
        }
      }
    }
    else {
      if (status_ == GRPC_STATUS_OK) {
        reply_stream_promise_.complete();
      } else {
        auto str = grpc_slice_to_c_string(status_details_);
        reply_stream_promise_.set_exception(std::make_exception_ptr(Rpc_error(status_, str)));
        gpr_free(str);
      }
    }

    return all_done;
  }

  grpc_call* call_;
  Stream_promise<RepT> reply_stream_promise_;
  grpc_byte_buffer* recv_buffer_ = nullptr;

  bool finishing_ = false;
  grpc_metadata_array server_metadata_;
  grpc_metadata_array trailing_metadata_;
  grpc_status_code status_;
  grpc_slice status_details_;
  const char* error_string_;
};
}  // namespace detail

template <typename RepT, typename ReqT>
Stream_future<RepT> start_server_streaming_call(Channel* channel, void* tag, const ReqT& req, Call_options options) {
 assert(options.completion_queue);

  auto call = grpc_channel_create_registered_call(
      channel->handle(), nullptr, GRPC_PROPAGATE_DEFAULTS,
      options.completion_queue->handle(), tag, options.deadline, nullptr);
  auto completion = new detail::Streaming_call_session<RepT>(call);
  auto send_buffer = serialize(req);

  std::array<grpc_op, 4> ops;

  ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
  ops[0].flags = 0;
  ops[0].reserved = nullptr;
  ops[0].data.send_initial_metadata.count = 0;
  ops[0].data.send_initial_metadata.maybe_compression_level.is_set = 0;

  ops[1].op = GRPC_OP_SEND_MESSAGE;
  ops[1].flags = 0;
  ops[1].reserved = nullptr;
  ops[1].data.send_message.send_message = send_buffer;

  ops[2].op = GRPC_OP_RECV_INITIAL_METADATA;
  ops[2].flags = 0;
  ops[2].reserved = 0;
  ops[2].data.recv_initial_metadata.recv_initial_metadata =
      &completion->server_metadata_;

  ops[3].op = GRPC_OP_RECV_MESSAGE;
  ops[3].flags = 0;
  ops[3].reserved = 0;
  ops[3].data.recv_message.recv_message = &completion->recv_buffer_;
  
  auto status =
      grpc_call_start_batch(completion->call_, ops.data(), ops.size(), completion->completion_tag(false).data, nullptr);

  if (status != GRPC_CALL_OK) {
    completion->reply_stream_promise_.set_exception(std::make_exception_ptr(error::internal("failed to start call")));
  }

  grpc_byte_buffer_destroy(send_buffer);

  return completion->reply_stream_promise_.get_future();
}


//*********************************************************************************//

// This is not quite as nice as it could be...
//
template<typename RepT, typename ReqT>
class Client_streaming_call_session final 
  : public Completion_callback {
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

    grpc_call_start_batch(call_, pending_ops.data(), pending_ops.size(), completion_tag().data, nullptr);
    batch_in_flight_ = true;

    req_.get_future().for_each([this](ReqT req){
      std::lock_guard l(mtx_);

      auto buffer = serialize(req);
      
      grpc_op op;
      op.op = GRPC_OP_SEND_MESSAGE;
      op.flags = 0;
      op.reserved = nullptr;
      op.data.send_message.send_message = buffer;

      pending_ops_.push(op);

      // Send whatever we have.
      flush_();
    }).finally([this](expected<void>) {
      std::lock_guard l(mtx_);
      finished_ = true;
      
      if(!batch_in_flight_) {
        send_end();
      }
    });
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
      grpc_call_start_batch(call_, &op, 1, completion_tag().data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);
    
    grpc_byte_buffer_destroy(op.data.send_message.send_message);
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
      grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);

    end_sent_ = true;

  }

  bool exec(bool, std::bitset<4>) noexcept override {
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

  Stream_promise<ReqT> req_;
  Promise<RepT> rep_;
  
  grpc_byte_buffer* recv_buffer_;

  grpc_metadata_array trailing_metadata_;
  grpc_status_code status_;
  grpc_slice status_details_;
  const char* error_string_;

  std::queue<grpc_op> pending_ops_;
};

template <typename RepT, typename ReqT>
std::tuple<Stream_promise<ReqT>, Future<RepT>> start_client_streaming_call(Channel* channel, void* tag, Call_options options) {
  assert(options.completion_queue);

  auto call = grpc_channel_create_registered_call(
      channel->handle(), nullptr, GRPC_PROPAGATE_DEFAULTS,
      options.completion_queue->handle(), tag, options.deadline, nullptr);

  auto call_session = new Client_streaming_call_session<RepT, ReqT>(call);  

  return {std::move(call_session->req_), call_session->rep_.get_future()};
}

//*********************************************************************************//

template<typename RepT, typename ReqT>
class Bidir_streaming_call_session final 
  : public Completion_callback {
public:
  Bidir_streaming_call_session(grpc_call* call, Stream_future<ReqT> req_stream) 
    : call_(call) {
    grpc_metadata_array_init(&trailing_metadata_);
    grpc_metadata_array_init(&server_metadata_);

    // Launch the metadata exchange
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

    can_send_ = false;
    grpc_call_start_batch(call_, pending_ops.data(), pending_ops.size(), completion_tag(2).data, nullptr);

    // Get ready to send requests
    req_stream.for_each([this](ReqT req){
      std::lock_guard l(mtx_);

      auto buffer = serialize(req);
      
      grpc_op op;
      op.op = GRPC_OP_SEND_MESSAGE;
      op.flags = 0;
      op.reserved = nullptr;
      op.data.send_message.send_message = buffer;

      pending_ops_.push(op);

      // Send whatever we have.
      flush_();
    }).finally([this](expected<void>) {
      std::lock_guard l(mtx_);
      finished_sending_ = true;
      
      if(can_send_) {
        send_client_end();
      }
    });
  }

  ~Bidir_streaming_call_session() {
    assert(pending_ops_.empty());

    grpc_metadata_array_destroy(&server_metadata_);
    grpc_metadata_array_destroy(&trailing_metadata_);

    if (recv_buffer_) {
      grpc_byte_buffer_destroy(recv_buffer_);
    }

    grpc_call_unref(call_);
  }

  void flush_() {
    if(!can_send_) {
      return ;
    }

    can_send_ = false;

    auto op = pending_ops_.front();
    pending_ops_.pop();

    assert(op.op == GRPC_OP_SEND_MESSAGE);
    
    auto status =
      grpc_call_start_batch(call_, &op, 1, completion_tag(1).data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
      assert(false);
    }
    
    grpc_byte_buffer_destroy(op.data.send_message.send_message);
  }


  void send_client_end() {
    std::array<grpc_op, 1> ops;

    ops[0].op = GRPC_OP_SEND_CLOSE_FROM_CLIENT;
    ops[0].flags = 0;
    ops[0].reserved = 0;

    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag(4).data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);

    end_sent_ = true;
  }

  void finish() {
    std::array<grpc_op, 1> ops;

    ops[0].op = GRPC_OP_RECV_STATUS_ON_CLIENT;
    ops[0].flags = 0;
    ops[0].reserved = 0;
    ops[0].data.recv_status_on_client.trailing_metadata =
        &trailing_metadata_;
    ops[0].data.recv_status_on_client.status = &status_;
    ops[0].data.recv_status_on_client.status_details =
        &status_details_;
    ops[0].data.recv_status_on_client.error_string = &error_string_;


    auto status =
      grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag(8).data, nullptr);

    if(status != GRPC_CALL_OK) {
      std::cerr << grpc_call_error_to_string(status) << "\n";
    }
    assert(status == GRPC_CALL_OK);

    end_sent_ = true;
  }

  bool exec(bool, std::bitset<4> flags) noexcept override {
    bool write_op = flags.test(0);
    bool handshake = flags.test(1);
    bool ending = flags.test(2);
    bool closing = flags.test(3);

    if(closing) {
      return true;
    }
    
    std::unique_lock l(mtx_);

    if(handshake) {
      // Allow the sending of messages
      can_send_ = true;
      if(!pending_ops_.empty()) {
        flush_();
      }

      // Start receiving messages from the server
      std::array<grpc_op, 1> ops;
      ops[0].op = GRPC_OP_RECV_MESSAGE;
      ops[0].flags = 0;
      ops[0].reserved = 0;
      ops[0].data.recv_message.recv_message = &recv_buffer_;

      auto call_status =
          grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

      if (call_status != GRPC_CALL_OK) {
        assert(false);
      }

      l.unlock();      
    }
    else if(ending) {
      if(finished_receiving_) {
        finish();
      }
    }
    else if(write_op) {
      // That was the end of a write op
      can_send_ = true;

      if(!pending_ops_.empty()) {
        flush_();
      }
      else if(finished_sending_) {
        send_client_end();
      }
    }
    else {
      // That was the end of a read op
      if( recv_buffer_) {
        auto data = recv_buffer_;
        recv_buffer_ = nullptr;

        std::array<grpc_op, 1> ops;

        ops[0].op = GRPC_OP_RECV_MESSAGE;
        ops[0].flags = 0;
        ops[0].reserved = 0;
        ops[0].data.recv_message.recv_message = &recv_buffer_;

        auto call_status =
            grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

        if (call_status != GRPC_CALL_OK) {
          assert(false);
        }

        rep_.push(deserialize<RepT>(data));
        grpc_byte_buffer_destroy(data);
      }
      else {
        finished_receiving_ = true;

        //TODO: This is not quite right, I think. We should wait for the status to be received from the server.
        rep_.complete();

        if(finished_sending_ && end_sent_) {
          finish();
        }
      }
    }
    return false;
  }

  Stream_promise<RepT> rep_;

  // I really wish we could do this without a mutex, somehow...
  std::mutex mtx_;
  bool can_send_ = false;
  bool finished_sending_ = false;
  bool finished_receiving_ = false;

  bool end_sent_ = false;

  grpc_call* call_;
  grpc_metadata_array server_metadata_;
  grpc_byte_buffer* recv_buffer_;


  grpc_metadata_array trailing_metadata_;
  grpc_status_code status_;
  grpc_slice status_details_;
  const char* error_string_;

  std::queue<grpc_op> pending_ops_;
};

template <typename RepT, typename ReqT>
std::tuple<Stream_promise<ReqT>, Stream_future<RepT>> start_bidir_streaming_call(Channel* channel, void* tag, Call_options options) {
  assert(options.completion_queue);

  auto call = grpc_channel_create_registered_call(
      channel->handle(), nullptr, GRPC_PROPAGATE_DEFAULTS,
      options.completion_queue->handle(), tag, options.deadline, nullptr);

  Stream_promise<ReqT> req;
  auto call_session = new Bidir_streaming_call_session<RepT, ReqT>(call, req.get_future());  

  return {std::move(req), call_session->rep_.get_future()};
}


}  // namespace client

}  // namespace easy_grpc
#endif
