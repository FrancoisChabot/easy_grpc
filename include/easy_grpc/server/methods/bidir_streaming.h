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

#ifndef EASY_GRPC_SERVER_METHOD_BIDIR_STREAMING_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_BIDIR_STREAMING_H_INCLUDED

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
class Bidir_streaming_call_handler 
  : public Completion_callback {

  Stream_promise<ReqT> reader_prom_;
  Stream_future<RepT> reply_fut_;

public:

  Bidir_streaming_call_handler() {
    grpc_metadata_array_init(&request_metadata_);
    grpc_metadata_array_init(&server_metadata_);
  }

  ~Bidir_streaming_call_handler() {
    grpc_metadata_array_destroy(&request_metadata_);
    grpc_metadata_array_destroy(&server_metadata_);
  }

  template<typename CbT>
  void perform(const CbT& cb) {      
      reply_fut_ = cb(reader_prom_.get_future());

      std::array<grpc_op, 2> ops;
      ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
      ops[0].flags = 0;
      ops[0].reserved = 0;
      ops[0].data.send_initial_metadata.count = server_metadata_.count;
      ops[0].data.send_initial_metadata.metadata = server_metadata_.metadata;
      ops[0].data.send_initial_metadata.maybe_compression_level.is_set = false;

      ops[1].op = GRPC_OP_RECV_MESSAGE;
      ops[1].flags = 0;
      ops[1].reserved = 0;
      ops[1].data.recv_message.recv_message = &payload_;

      auto call_status =
          grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

      if (call_status != GRPC_CALL_OK) {
        std::cerr << grpc_call_error_to_string(call_status) << "\n";
        assert(false);  // TODO: HANDLE THIS
      }
  }

  void finish(expected<RepT> rep) {
    finishing = true; 
    if (rep.has_value()) {
      std::array<grpc_op, 3> ops;

      auto buffer = serialize(rep.value());
      ops[0].op = GRPC_OP_SEND_MESSAGE;
      ops[0].flags = 0;
      ops[0].reserved = nullptr;
      ops[0].data.send_message.send_message = buffer;

      ops[1].op = GRPC_OP_SEND_STATUS_FROM_SERVER;
      ops[1].flags = 0;
      ops[1].reserved = nullptr;
      ops[1].data.send_status_from_server.trailing_metadata_count = 0;
      ops[1].data.send_status_from_server.trailing_metadata = nullptr;
      ops[1].data.send_status_from_server.status = GRPC_STATUS_OK;
      ops[1].data.send_status_from_server.status_details = nullptr;

      ops[2].op = GRPC_OP_RECV_CLOSE_ON_SERVER;
      ops[2].flags = 0;
      ops[2].reserved = nullptr;
      ops[2].data.recv_close_on_server.cancelled = &cancelled_;

      auto call_status =
          grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

      grpc_byte_buffer_destroy(buffer);

      if (call_status != GRPC_CALL_OK) {
        assert(false);  // TODO: HANDLE THIS
      }

    } else {
      // The call has failed.
      std::array<grpc_op, 2> ops;

      grpc_status_code status = GRPC_STATUS_UNKNOWN;
      grpc_slice details = grpc_empty_slice();

      // If we can extract an error code,2and message, do so.
      try {
        std::rethrow_exception(rep.error());
      } catch (Rpc_error& e) {
        status = e.code();
        details = grpc_slice_from_copied_string(e.what());
      } catch (...) {
      }
      ops[0].op = GRPC_OP_SEND_STATUS_FROM_SERVER;
      ops[0].flags = 0;
      ops[0].reserved = nullptr;
      ops[0].data.send_status_from_server.trailing_metadata_count = 0;
      ops[0].data.send_status_from_server.trailing_metadata = nullptr;
      ops[0].data.send_status_from_server.status = status;
      ops[0].data.send_status_from_server.status_details = &details;

      ops[1].op = GRPC_OP_RECV_CLOSE_ON_SERVER;
      ops[1].flags = 0;
      ops[1].reserved = nullptr;
      ops[1].data.recv_close_on_server.cancelled = &cancelled_;

      auto call_status =
          grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

      grpc_slice_unref(details);

      if (call_status != GRPC_CALL_OK) {
        assert(false);  // TODO: HANDLE THIS
      }
    }
  }

  bool exec(bool, bool) noexcept override {
    bool all_done = finishing;

    if(!all_done) {
      if(payload_) {
        auto raw_data = payload_;
        payload_ = nullptr;

        std::array<grpc_op, 1> ops;

        ops[0].op = GRPC_OP_RECV_MESSAGE;
        ops[0].flags = 0;
        ops[0].reserved = 0;
        ops[0].data.recv_message.recv_message = &payload_;

        auto call_status =
            grpc_call_start_batch(call_, ops.data(), ops.size(), completion_tag().data, nullptr);

        if (call_status != GRPC_CALL_OK) {
          assert(false);
        }

        reader_prom_.push(deserialize<ReqT>(raw_data));
        grpc_byte_buffer_destroy(raw_data);        
      }
      else {
        reader_prom_.complete();
        reply_fut_.finally([this](expected<value_type> rep) { 
          this->finish(std::move(rep)); 
        });
      }
    }

    return all_done;
  }

  bool finishing = false;
  int cancelled_ = false;
  grpc_call* call_ = nullptr;
  gpr_timespec deadline_;
  grpc_metadata_array request_metadata_;
  grpc_metadata_array server_metadata_;
  grpc_byte_buffer* payload_ = nullptr;
};


  
template <typename CbT>
class Bidir_streaming_call_listener : public Completion_callback {
    using InReaderT = typename function_traits<CbT>::template arg<0>::type;
    using OutWriterT = typename function_traits<CbT>::template arg<1>::type;
    using OutT = typename OutWriterT::value_type;
    using InT = typename InReaderT::value_type;

    using handler_type = Bidir_streaming_call_handler<InT, OutT>;
 public:
  Bidir_streaming_call_listener(grpc_server* server, void* registration,
                      grpc_completion_queue* cq, CbT cb)
      : srv_(server), reg_(registration), cq_(cq), cb_(std::move(cb)) {
    // It's really important that inject is not called here. As the object
    // could end up being deleted before it's fully constructed.
  }

  ~Bidir_streaming_call_listener() {
    if (pending_call_) {
      delete pending_call_;
    }
  }

  bool exec(bool success, bool alternate) noexcept override {
    EASY_GRPC_TRACE(Bidir_streaming_call_listener, exec);

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
class Bidir_streaming_method : public Method {
 public:
  Bidir_streaming_method(const char* name, CbT cb) : Method(name), cb_(cb) {}

  void listen(grpc_server* server, void* registration,
              grpc_completion_queue* cq) override {
    auto handler =
        new Bidir_streaming_call_listener<CbT>(server, registration, cq, cb_);
    handler->inject();
  }

  bool immediate_payload_read() const override {
    return false;
  }
 private:
  CbT cb_;
};

}
}
}
#endif