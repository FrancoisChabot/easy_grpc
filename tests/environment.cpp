#include "easy_grpc/easy_grpc.h"

#include "gtest/gtest.h"

namespace rpc = easy_grpc;

TEST(environment, validation) {
  EXPECT_FALSE(rpc::Environment::is_valid());

  {
    rpc::Environment env;
    EXPECT_TRUE(rpc::Environment::is_valid());
    rpc::Environment::assert_valid();
  }
  EXPECT_FALSE(rpc::Environment::is_valid());
}
