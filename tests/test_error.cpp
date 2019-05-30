#include "easy_grpc/easy_grpc.h"

#include "gtest/gtest.h"

namespace rpc = easy_grpc;

TEST(error, error_types) {
    EXPECT_EQ(rpc::error::cancelled("").code(), GRPC_STATUS_CANCELLED);
    EXPECT_EQ(rpc::error::unknown("").code(), GRPC_STATUS_UNKNOWN);
    EXPECT_EQ(rpc::error::invalid_argument("").code(), GRPC_STATUS_INVALID_ARGUMENT);
    EXPECT_EQ(rpc::error::deadline_exceeded("").code(), GRPC_STATUS_DEADLINE_EXCEEDED);
    EXPECT_EQ(rpc::error::not_found("").code(), GRPC_STATUS_NOT_FOUND);
    EXPECT_EQ(rpc::error::already_exists("").code(), GRPC_STATUS_ALREADY_EXISTS);
    EXPECT_EQ(rpc::error::permission_denied("").code(), GRPC_STATUS_PERMISSION_DENIED);
    EXPECT_EQ(rpc::error::unauthenticated("").code(), GRPC_STATUS_UNAUTHENTICATED);
    EXPECT_EQ(rpc::error::resource_exhausted("").code(), GRPC_STATUS_RESOURCE_EXHAUSTED);
    EXPECT_EQ(rpc::error::failed_precondition("").code(), GRPC_STATUS_FAILED_PRECONDITION);
    EXPECT_EQ(rpc::error::aborted("").code(), GRPC_STATUS_ABORTED);
    EXPECT_EQ(rpc::error::out_of_range("").code(), GRPC_STATUS_OUT_OF_RANGE);
    EXPECT_EQ(rpc::error::unimplemented("").code(), GRPC_STATUS_UNIMPLEMENTED);
    EXPECT_EQ(rpc::error::internal("").code(), GRPC_STATUS_INTERNAL);
    EXPECT_EQ(rpc::error::unavailable("").code(), GRPC_STATUS_UNAVAILABLE);
    EXPECT_EQ(rpc::error::data_loss("").code(), GRPC_STATUS_DATA_LOSS);
}

TEST(error, movable) {
    auto err = rpc::error::cancelled("");
    
    rpc::Rpc_error moved = std::move(err);
    EXPECT_EQ(moved.code(), GRPC_STATUS_CANCELLED);
}

