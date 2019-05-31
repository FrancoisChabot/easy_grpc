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

#ifndef EASY_GRPC_SERVER_SERVICE_IMPL_INCLUDED_H
#define EASY_GRPC_SERVER_SERVICE_IMPL_INCLUDED_H

#include "easy_grpc/completion_queue.h"
#include "easy_grpc/error.h"
#include "easy_grpc/serialize.h"
#include "easy_grpc/third_party/variadic_future.h"

#include "easy_grpc/config.h"

#include <iostream>

namespace easy_grpc {

namespace server {
  class Server;
  namespace detail {

    template<typename ReqT, typename RepT>
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

      template<typename HandlerT>
      void perform(const HandlerT& handler) {
        assert(payload_);
        auto req = deserialize<ReqT>(payload_);
        grpc_byte_buffer_destroy(payload_);
        Future<RepT> result;
        try {
          result = handler(req);
        }
        catch(...) {
          result = Future<RepT>(std::current_exception());
        }

        result.then_finally_expect([this](expected<RepT> rep){
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
              std::cerr << "something went wrong...\n";
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
              std::cerr << "something went wrong...\n";
            }
            
          }
        });

      }

      int cancelled_ = false;
      grpc_call* call_ = nullptr;
      gpr_timespec deadline_;
      grpc_metadata_array request_metadata_;
      grpc_metadata_array server_metadata_;
      grpc_byte_buffer* payload_ = nullptr;
    };
    
    template<typename ReqT, typename RepT, typename HandlerT>
    class Unary_call_listener : public Completion_queue::Completion {
    public:
      Unary_call_listener(grpc_server* server, void* registration, grpc_completion_queue* cq, HandlerT handler)
        : srv_(server)
        , reg_(registration)
        , cq_(cq)
        , handler_(std::move(handler)){
          
          // It's really important that inject is not called here. As the object
          // could end up being deleted before it's fully constructed.
        }

      ~Unary_call_listener() {
        if(pending_call_) {
          delete pending_call_;
        }
      }
      bool exec(bool success) noexcept override {
        EASY_GRPC_TRACE(Unary_call_listener, exec);

        if(success) {
          
          pending_call_->perform(handler_);
          pending_call_ = nullptr;

          // Listen for a new call.
          inject();
          return false; // This object is recycled.
        }

        return true;
       }

      void inject() {
        EASY_GRPC_TRACE(Unary_call_listener, inject);

        pending_call_ = new Unary_call_handler<ReqT, RepT>;
        auto status = grpc_server_request_registered_call(
          srv_, reg_, &pending_call_->call_,
          &pending_call_->deadline_, &pending_call_->request_metadata_,
          &pending_call_->payload_, cq_, cq_, this);
      }

      HandlerT handler_;
      grpc_server* srv_;
      void* reg_;
      grpc_completion_queue* cq_;

      Unary_call_handler<ReqT, RepT>* pending_call_ = nullptr;

    };

    class Method {
    public:
      Method(const char* name) : name_(name) {}
      virtual ~Method() {}

      const char* name() const { return name_;}

      void set_queues(Completion_queue_set queues) {queues_ = queues;}
      const Completion_queue_set& queues() const {return queues_;}

      virtual void listen(grpc_server* server, void* registration, grpc_completion_queue* cq) = 0;

    private:
      Completion_queue_set queues_;
      const char* name_;
    };

    template<typename ReqT, typename RepT, typename HandlerT>
    class Unary_method : public Method {
    public:
      Unary_method(const char * name, HandlerT method_handler) : Method(name), method_handler_(method_handler) {}

      void listen(grpc_server* server, void* registration, grpc_completion_queue* cq) override {
        auto handler = new Unary_call_listener<ReqT, RepT, HandlerT>(server, registration, cq, method_handler_);
        handler->inject();
      }

    private:
      HandlerT method_handler_;
    };


    template<typename ReqT, typename RepT, typename HandlerT>
    auto make_unary_method(const char* name, HandlerT handler) {
      return std::make_unique<Unary_method<ReqT, RepT, HandlerT >>(name, std::move(handler));
    }

    class Method_visitor {
      public:
        virtual ~Method_visitor() {}
        virtual void visit(Method&) = 0;
    };

  }
}

}  // namespace easy_grpc
#endif