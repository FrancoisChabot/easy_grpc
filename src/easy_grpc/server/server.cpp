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

#include "easy_grpc/server/server.h"
#include "easy_grpc/server/service.h"
#include "easy_grpc/server/service_impl.h"

#include <set>
#include <cassert>

namespace easy_grpc {

namespace server {

  namespace {
    class Service_binder : public detail::Method_visitor {
    public:
      Service_binder(Server* tgt, const Completion_queue_set& default_queues)
        : default_queues_(default_queues)
        , tgt_(tgt) {}

      void visit(detail::Method& info) override {
        auto handle = grpc_server_register_method(
          tgt_->handle(), info.name(), nullptr,
          GRPC_SRM_PAYLOAD_READ_INITIAL_BYTE_BUFFER,
          0);

          if(!handle) {
            throw std::runtime_error("failed to register method");
          }

          Completion_queue_set queues = info.queues();
          if(queues.empty()) {
            queues = default_queues_;
          }

          assert(!queues.empty());
          ops_.push_back({handle, &info, std::move(queues)});
      }

      Server* tgt_;

      struct Method_reg_op {
        void* handle_;
        detail::Method* method_;
        Completion_queue_set queues_;
      };

      const Completion_queue_set& default_queues_;
      std::vector<Method_reg_op> ops_;
    };
  }


Server::Server(const Config& cfg) 
  : default_queues_(cfg.default_queues_) { 

  // We need to pre-allocate the shutdown queue. Because it must be registered in the server prior to starting it.
  grpc_completion_queue_attributes sd_queue_attribs;
  sd_queue_attribs.version = GRPC_CQ_CURRENT_VERSION;
  sd_queue_attribs.cq_completion_type = GRPC_CQ_NEXT;
  sd_queue_attribs.cq_polling_type = GRPC_CQ_NON_POLLING;
  sd_queue_attribs.cq_shutdown_cb = nullptr;

  shutdown_queue_ = grpc_completion_queue_create(grpc_completion_queue_factory_lookup(&sd_queue_attribs), &sd_queue_attribs, nullptr);


  impl_ = grpc_server_create(nullptr, nullptr); 

  add_listening_ports_(cfg);

  std::vector<Service_binder::Method_reg_op> all_methods;

  for(const auto& service: cfg.services_) {
    Completion_queue_set service_default_queues = service.service->default_queues();
    if(service_default_queues.empty()) {
      service_default_queues = default_queues_;
    }

    Service_binder bind_iface(this, service_default_queues);
    service.service->visit_methods(bind_iface);
    all_methods.insert(all_methods.end(), std::make_move_iterator(bind_iface.ops_.begin()), std::make_move_iterator(bind_iface.ops_.end()));
  }

  // Put queues in a global set so that we can dedup them for registration.
  std::set<grpc_completion_queue*> queues_to_register;

  for(auto& m : all_methods) {
    for(auto& cq : m.queues_) {
      queues_to_register.insert(cq.get().handle());
    }
  }

  for(auto cq : queues_to_register) {
    grpc_server_register_completion_queue(impl_, cq, nullptr);
  }


  grpc_server_register_completion_queue(impl_, shutdown_queue_, nullptr);
  

  grpc_server_start(impl_);

  // Start handling the methods

  for(auto& m : all_methods) {
    for(auto& cq : m.queues_) {
      m.method_->listen(impl_, m.handle_, cq.get().handle());
    }
  }
}

void Server::add_listening_ports_(const Config& cfg) {
  for(const auto& port: cfg.ports_) {
    if(!port.creds) {
      auto bound_port = grpc_server_add_insecure_http2_port(impl_, port.addr.c_str());

      if(!bound_port) {
        cleanup_();
        throw std::runtime_error("grpc_server_add_insecure_http2_port failed");
      }
      if(port.bound_report) {
        *port.bound_report = bound_port;
      }
    }
    else {
      assert(false); // Unimplemented
    }
  }
}


Server::~Server() { cleanup_(); }

Server::Server(Server&& rhs) : impl_(rhs.impl_), shutdown_queue_(rhs.shutdown_queue_) { rhs.impl_ = nullptr; rhs.shutdown_queue_= nullptr;}

Server& Server::operator=(Server&& rhs) {
  cleanup_();
  impl_ = rhs.impl_;
  shutdown_queue_ = rhs.shutdown_queue_;

  rhs.shutdown_queue_ = nullptr;
  rhs.impl_ = nullptr;

  return *this;
}

void Server::cleanup_() {
  if (impl_) {
    // Perform a synchronous server shutdown.
    grpc_server_shutdown_and_notify(impl_, shutdown_queue_, nullptr);
    auto evt = grpc_completion_queue_next(shutdown_queue_, gpr_inf_future(GPR_CLOCK_REALTIME), nullptr);
    assert(evt.type == GRPC_OP_COMPLETE);
    grpc_completion_queue_shutdown(shutdown_queue_);

    // Consume the shutdown event.
    evt = grpc_completion_queue_next(shutdown_queue_, gpr_inf_future(GPR_CLOCK_REALTIME), nullptr);
    assert(evt.type == GRPC_QUEUE_SHUTDOWN);
    grpc_completion_queue_destroy(shutdown_queue_);

    // destroy the server.
    grpc_server_destroy(impl_);
    
  }
}
}  // namespace server

}  // namespace easy_grpc