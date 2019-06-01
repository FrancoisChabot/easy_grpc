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

#ifndef EASY_GRPC_SERVER_METHOD_UNARY_HANDLER_H_INCLUDED
#define EASY_GRPC_SERVER_METHOD_UNARY_HANDLER_H_INCLUDED

#include "easy_grpc/config.h"
#include "easy_grpc/error.h"
#include "easy_grpc/serialize.h"
#include "easy_grpc/third_party/function_traits.h"
#include "easy_grpc/third_party/variadic_future.h"

#include "grpc/grpc.h"

#include <cassert>

namespace easy_grpc {
namespace server {
namespace detail {
  
    template<typename RepT>
    class Unary_call_handler : public Completion_queue::Completion {
    public:
      Unary_call_handler() {
        grpc_metadata_array_init(&request_metadata_);
        grpc_metadata_array_init(&server_metadata_);
      }

      bool exec(bool success) noexcept override {
        grpc_metadata_array_destroy(&request_metadata_);
        grpc_metadata_array_destroy(&server_metadata_);

        grpc_call_unref(call_);
        return true;
      }

      void finish(expected<RepT> rep) {
        if(rep.has_value()) {
          std::array<grpc_op, 4> ops;

          ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
          ops[0].flags = 0;
          ops[0].reserved = 0;
          ops[0].data.send_initial_metadata.count = server_metadata_.count;
          ops[0].data.send_initial_metadata.metadata = server_metadata_.metadata;
          ops[0].data.send_initial_metadata.maybe_compression_level.is_set = false;
          
          auto buffer = serialize(rep.value());
          ops[1].op = GRPC_OP_SEND_MESSAGE;
          ops[1].flags = 0;
          ops[1].reserved = nullptr;
          ops[1].data.send_message.send_message = buffer;

          ops[2].op = GRPC_OP_SEND_STATUS_FROM_SERVER;
          ops[2].flags = 0;
          ops[2].reserved = nullptr;
          ops[2].data.send_status_from_server.trailing_metadata_count = 0;
          ops[2].data.send_status_from_server.trailing_metadata = nullptr;
          ops[2].data.send_status_from_server.status = GRPC_STATUS_OK;
          ops[2].data.send_status_from_server.status_details = nullptr;

          ops[3].op = GRPC_OP_RECV_CLOSE_ON_SERVER;
          ops[3].flags = 0;
          ops[3].reserved = nullptr;
          ops[3].data.recv_close_on_server.cancelled = &cancelled_;

          auto call_status = grpc_call_start_batch(call_, ops.data(), ops.size(), this, nullptr);

          grpc_byte_buffer_destroy(buffer);

          if(call_status != GRPC_CALL_OK) {
            assert(false); //TODO: HANDLE THIS
          }
          
        }
        else {
          // The call has failed.
          std::array<grpc_op, 3> ops;

          grpc_status_code status = GRPC_STATUS_UNKNOWN;
          grpc_slice details = grpc_empty_slice();

          // If we can extract an error code, and message, do so.
          try {
            std::rethrow_exception( rep.error());
          }
          catch(Rpc_error& e) {
            status = e.code();
            details = grpc_slice_from_copied_string(e.what());
          }
          catch(...) {}

          ops[0].op = GRPC_OP_SEND_INITIAL_METADATA;
          ops[0].flags = 0;
          ops[0].reserved = 0;
          ops[0].data.send_initial_metadata.count = server_metadata_.count;
          ops[0].data.send_initial_metadata.metadata = server_metadata_.metadata;
          ops[0].data.send_initial_metadata.maybe_compression_level.is_set = false;
              
          ops[1].op = GRPC_OP_SEND_STATUS_FROM_SERVER;
          ops[1].flags = 0;
          ops[1].reserved = nullptr;
          ops[1].data.send_status_from_server.trailing_metadata_count = 0;
          ops[1].data.send_status_from_server.trailing_metadata = nullptr;
          ops[1].data.send_status_from_server.status = status;
          ops[1].data.send_status_from_server.status_details = &details;

          ops[2].op = GRPC_OP_RECV_CLOSE_ON_SERVER;
          ops[2].flags = 0;
          ops[2].reserved = nullptr;
          ops[2].data.recv_close_on_server.cancelled = &cancelled_;

          auto call_status = grpc_call_start_batch(call_, ops.data(), ops.size(), this, nullptr);

          grpc_slice_unref(details);

          if(call_status != GRPC_CALL_OK) {
            assert(false); //TODO: HANDLE THIS
          }
        }
      }


      int cancelled_ = false;
      grpc_call* call_ = nullptr;
      gpr_timespec deadline_;
      grpc_metadata_array request_metadata_;
      grpc_metadata_array server_metadata_;
      grpc_byte_buffer* payload_ = nullptr;
    };


    template<typename ReqT, typename RepT>
    class Unary_sync_call_handler : public Unary_call_handler<RepT> {
    public:
      template<typename HandlerT>
      void perform(const HandlerT& handler) {
        assert(this->payload_);
        auto req = deserialize<ReqT>(this->payload_);
        grpc_byte_buffer_destroy(this->payload_);
        expected<RepT> result;
        try {
          result = handler(req);
        }
        catch(...) {
          result = unexpected{std::current_exception()};
        }

        this->finish(std::move(result));
      }

    };



    template<typename ReqT, typename RepT>
    class Unary_async_call_handler : public Unary_call_handler<typename RepT::value_type> {
    public:
      using value_type = typename RepT::value_type;

      template<typename HandlerT>
      void perform(const HandlerT& handler) {
        assert(this->payload_);
        auto req = deserialize<ReqT>(this->payload_);
        grpc_byte_buffer_destroy(this->payload_);
        RepT result;
        try {
          result = handler(req);
        }
        catch(...) {
          result = RepT(std::current_exception());
        }

        result.then_finally_expect([this](expected<value_type> rep){
            this->finish(std::move(rep));
          });
      }
    };
}
}
}
#endif