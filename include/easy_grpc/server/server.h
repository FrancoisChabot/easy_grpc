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

#ifndef EASY_GRPC_SERVER_SERVER_H_INCLUDED
#define EASY_GRPC_SERVER_SERVER_H_INCLUDED

#include "easy_grpc/completion_queue.h"
#include "easy_grpc/server/credentials.h"
#include "easy_grpc/server/service_config.h"

#include "grpc/grpc.h"

#include <memory>
#include <string>
#include <vector>

namespace easy_grpc {

class Completion_queue;
namespace server {

class Service;
class Config;

class Feature {
  public:
    virtual ~Feature() {}

    virtual void add_to_config(Config&) = 0;
};

class Config {
 public:
  Config() = default;
  Config(const Config&) = delete;
  Config(Config&&) = default;
  Config& operator=(Config&&) = default;
  Config& operator=(const Config&) = delete;

  Config& add_default_listening_queues(Completion_queue_set) &;
  Config&& add_default_listening_queues(Completion_queue_set set) &&;

  template <typename ServiceT>
  Config& add_service(ServiceT& service)& {
    using service_type = typename ServiceT::service_type;

    return add_service(service_type::get_config(service));
  }

  template <typename ServiceT>
  Config&& add_service(ServiceT& service)&& {
    using service_type = typename ServiceT::service_type;

    return std::move(add_service(service_type::get_config(service)));
  }

  template<typename FeatureType>
  Config& add_feature(FeatureType f) & {
    features_.push_back(std::make_unique<FeatureType>(std::move(f)));
    return *this;
  }

  template<typename FeatureType>
  Config&& add_feature(FeatureType f) && {
    features_.push_back(std::make_unique<FeatureType>(std::move(f)));
    return std::move(*this);
  }

  Config& add_service(Service_config)&;
  Config&& add_service(Service_config)&&;

  Config& add_listening_port(std::string addr,
                              std::shared_ptr<Credentials> creds = {},
                              int* bound_port = nullptr) &;

  Config&& add_listening_port(std::string addr,
                            std::shared_ptr<Credentials> creds = {},
                            int* bound_port = nullptr) &&;

  const std::vector<Service_config>& get_services() const;

 private:
  struct Port {
    std::string addr;
    std::shared_ptr<Credentials> creds;
    int* bound_report;
  };

  Completion_queue_set default_queues_;
  std::vector<Service_config> service_cfgs_;
  std::vector<Port> ports_;
  std::vector<std::unique_ptr<Feature>> features_;
  friend class Server;
};

class Server {
 public:
  Server() = default;
  Server(Config cfg);
  Server(Server&& rhs);
  Server& operator=(Server&& rhs);
  ~Server();

  grpc_server* handle() { return impl_; }

 private:
  void add_listening_ports_(const Config& cfg);
  void cleanup_();

  // Noncopyable
  Server(const Server&) = delete;
  Server& operator=(const Server&) = delete;

  grpc_server* impl_ = nullptr;
  Completion_queue_set default_queues_;

  grpc_completion_queue* shutdown_queue_ = nullptr;

  std::vector<std::unique_ptr<Feature>> features_;
};
}  // namespace server

}  // namespace easy_grpc

#endif