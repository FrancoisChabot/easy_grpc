#ifndef EASY_GRPC_REFLECTION_INCLUDED_H
#define EASY_GRPC_REFLECTION_INCLUDED_H

#include "easy_grpc/server/server.h"

namespace easy_grpc {
  namespace reflection {
    void enable_reflection(::easy_grpc::server::Config& cfg);
  }
}

#endif