#ifndef EASY_GRPC_REFLECTION_INCLUDED_H
#define EASY_GRPC_REFLECTION_INCLUDED_H

#include "easy_grpc/server/server.h"

namespace easy_grpc {
  class Reflection_impl;
  class Reflection_feature : public server::Feature {
    public:
      Reflection_feature();
      Reflection_feature(Reflection_feature&&);
      Reflection_feature& operator=(Reflection_feature&&);
      
      ~Reflection_feature();

      void add_to_config(server::Config&) override;

    private:
      std::unique_ptr<Reflection_impl> impl_;
  };
}

#endif