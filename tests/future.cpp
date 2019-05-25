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

#include "easy_grpc/variadic_future.h"

#include "gtest/gtest.h"

namespace rpc = easy_grpc;

TEST(Future, fill_from_promise_st) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  prom.set_value(1);

  int dst = 0;
  fut.then_finally([&](int v) {dst = v;});
   
  EXPECT_EQ(dst, 1);
}


TEST(Future, fill_from_promise_mt) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  std::atomic<int> dst = 0;
  fut.then_finally([&](int v) { dst = v; });

  std::thread thread([prom=std::move(prom)]() mutable {
    prom.set_value(1);
  });

  thread.join();

  EXPECT_EQ(dst, 1);
}

TEST(Future, fill_from_void_promise_mt) {
  rpc::Promise<> prom;
  auto fut = prom.get_future();

  std::atomic<int> dst = 0;
  fut.then_finally([&]() { dst = 4; });

  std::thread thread([prom=std::move(prom)]() mutable {
    prom.set_value();
  });

  thread.join();

  EXPECT_EQ(dst, 4);
}

TEST(Future, simple_get) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  std::atomic<int> dst = 0;

  std::thread thread([&]() mutable {
    prom.set_value(2);
  });

  EXPECT_EQ(2, fut.get());

  thread.join();  
}

TEST(Future, simple_get_void) {
  rpc::Promise<> prom;
  auto fut = prom.get_future();

  std::atomic<int> dst = 0;

  std::thread thread([&]() mutable {
    prom.set_value();
  });

  fut.get();

  thread.join();  
}


// The future should still be valid even if the original thread is long gone.
TEST(Future, get_after_join) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  std::atomic<int> dst = 0;

  std::thread thread([&]() mutable {
    prom.set_value(2);
  });

  thread.join();

  EXPECT_EQ(2, fut.get());
}

TEST(Future, set_failure_st) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  try {
    throw std::runtime_error("nope");
  }
  catch(...) {
    prom.set_exception(std::current_exception());
  }

  EXPECT_THROW(fut.get(), std::runtime_error);
}

TEST(Future, set_failure_mt) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  std::thread thread([&]() {
    try {
      throw std::runtime_error("nope");
    }
    catch(...) {
      prom.set_exception(std::current_exception());
    }
  });

  EXPECT_THROW(fut.get(), std::runtime_error);

  thread.join();  
}


TEST(Future, expect_success) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future()
    .then_expect([](rpc::expected<int> e) { return *e + 2; });

  prom.set_value(5);
  EXPECT_EQ(fut.get(), 7);
}

TEST(Future, expect_void_success) {
  rpc::Promise<> prom;
  auto fut = prom.get_future()
    .then_expect([](rpc::expected<void> e) { return e.has_value() ? 1 : 2; });

  prom.set_value();
  EXPECT_EQ(fut.get(), 1);
}

TEST(Future, expect_void_success_to_void) {
  rpc::Promise<> prom;
  auto fut = prom.get_future()
    .then_expect([](rpc::expected<void> e) { });

  prom.set_value();
}

TEST(Future, expect_failure) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future()
    .then_expect([](rpc::expected<int> e) { return e.has_value() ? 3 : 4; });

  try {
    throw std::runtime_error("");
  }
  catch(...) {
    prom.set_exception(std::current_exception());
  }
  EXPECT_EQ(fut.get(), 4);
}

TEST(Future, expect_void_failure) {
  rpc::Promise<> prom;
  auto fut = prom.get_future()
    .then_expect([](rpc::expected<void> e) { return e.has_value() ? 1 : 2; });

  try {
    throw std::runtime_error("");
  }
  catch(...) {
    prom.set_exception(std::current_exception());
  }

  EXPECT_EQ(fut.get(), 2);
}

TEST(Future, finalize) {
  rpc::Promise<int> prom;

  int result = 0;
  prom.get_future()
    .then_finally([&](int v) { result = v; });


  prom.set_value(4);
  EXPECT_EQ(result, 4);
}

TEST(Future, expect_finalize) {
  rpc::Promise<int> prom;

  int result = 0;
  prom.get_future()
    .then_finally_expect([&](rpc::expected<int> v) { result = *v; });


  prom.set_value(4);
  EXPECT_EQ(result, 4);
}

TEST(Future, expect_finalize_failure) {
  rpc::Promise<int> prom;

  int result = 4;
  prom.get_future()
    .then_finally_expect([&](rpc::expected<int> v) { result = v.has_value() ? 0 : 1; });

  try {
    throw std::runtime_error("");
  }
  catch(...) {
    prom.set_exception(std::current_exception());
  }

  EXPECT_EQ(result, 1);
}

TEST(Future, catch_chain_failure) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  auto fut_2 = fut.then([](int) -> int {throw std::runtime_error("yo");});

  std::thread thread([&]() {
    prom.set_value(2);
  });

  EXPECT_THROW(fut_2.get(), std::runtime_error);

  thread.join();  
}

TEST(Future, intercept_chain_failure) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  auto fut_2 = fut.then([](int) -> int {throw std::runtime_error("yo");})
                  .then_expect([](rpc::expected<int> e) { return 5; });

  std::thread thread([&]() {
    prom.set_value(2);
  });

  EXPECT_EQ(fut_2.get(), 5);

  thread.join();  
}

TEST(Future, expected_chain) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  auto fut_2 = fut.then([](int v) -> int {return v * 4;})
                  .then_expect([](rpc::expected<int> e) { return *e * 2; });

  std::thread thread([&]() {
    prom.set_value(2);
  });

  EXPECT_EQ(fut_2.get(), 16);

  thread.join();  
}

TEST(Future, unexpected_catch) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  auto fut_2 = fut.then([](int v) -> int {throw std::runtime_error("yo");})

                  // e.value() while in error state should throw the original exception,
                  // which should get stored in fut_2
                  .then_expect([](rpc::expected<int> e) { return e.value() * 2; });

  std::thread thread([&]() {
    prom.set_value(2);
  });

  EXPECT_THROW(fut_2.get(), std::runtime_error);

  thread.join();  
}

TEST(Future, chain_from_promise_mt) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  std::atomic<int> dst = 0;
  fut
    .then([](int v) { return v * v; })
    .then([](int v) { return v + 3; })
    .then([](int v) { return v * v; })
    .then_finally([&](int v) { dst = v; });

  std::thread thread([prom=std::move(prom)]() mutable {
    prom.set_value(2);
  });

  thread.join();

  EXPECT_EQ(dst, 49);
}


// **************** MULTIPLE VALUES ************************* //

TEST(Multi_Future, fill_from_promise_st) {
  rpc::Promise<int, bool> prom;
  auto fut = prom.get_future();

  prom.set_value(1, true);

  int dst_i = 0;
  bool dst_b = false;

  fut.then_finally([&](int v, bool b) {dst_i = v; dst_b = b;});
   
  EXPECT_EQ(dst_i, 1);
  EXPECT_EQ(dst_b, true);
}


TEST(Multi_Future, simple_tie) {
  rpc::Promise<int> prom_i;
  rpc::Promise<bool> prom_b;

  rpc::Future<int> fut_i = prom_i.get_future();
  rpc::Future<bool> fut_b = prom_b.get_future();

  rpc::Future<int, bool> fut = tie(std::move(fut_i), std::move(fut_b));

  // This will be invoked when both futures have been completed.
  auto result_fut = fut.then([](int i, bool b){ return b ? i+3 : i-3;});

  prom_i.set_value(3);
  prom_b.set_value(true);

  EXPECT_EQ(result_fut.get(), 6);
}
/*
rpc::Future<int> foo(int x) {
  rpc::Promise<int> prom;

  auto result = prom.get_future();
  prom.set_value(x + 4);

  return result;
}

TEST(Future, then_returning_future) {
  rpc::Promise<int> prom;
  auto fut = prom.get_future();

  auto fut_2 = fut.then([](int v) { return foo(v); });

  prom.set_value(4);

  EXPECT_EQ(fut_2.get(), 8);
}
*/