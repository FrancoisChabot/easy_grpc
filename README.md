# easy_grpc

A future-based gRPC API for C++.

This project is an attempt to create a wrapper around gRPC to make
writing asynchronous servers and clients as straightforward as possible. It may not
be quite as optimal as a fully-customized server working on raw completion 
queues, but every effort is being made to get as close as possible while
maintaining as sane and modern an API as possible.

## The Goal

With easy_grpc, writing asynchronous rpc servers is simple. This is especially true for
servers that need to send rpcs to other services during the handling of a rpc.

```cpp
class MyService_impl : public pkg::MyService {
public:
  MyService_impl(pkg::MyService2::Stub_interface* stub2, 
                 pkg::MyService2::Stub_interface* stub3,
                 rpc::Work_queue* worker) 
      : stub2_(stub2)
      , stub3_(stub3) {}

  rpc::Future<pkg::Reply> MyMethod(pkg::Request req, rpc::Server_context ctx) {
    pkg::MyRequest2 stub2_request;
    pkg::MyRequest3 stub3_request;

    // These requests are sent in parallel.
    auto rep_2f = stub_2->Method2(stub2_request, ctx);
    auto rep_3f = stub_3->Method3(stub3_request, ctx);

    // Wait for the completion of both requests:
    return tie(rep_2f, rep_3f).then(
      [](auto rep_2, auto rep_3) {
        pkg::Reply reply;
        return reply;
      });
    )
  }

private:
  pkg::MyService2::Stub_interface* stub2_;
  pkg::MyService3::Stub_interface* stub3_;
};
```

Key points:
- The handling thread is freed while the child rpcs are in progess
- If either of the child rpcs fail, the failure is propagated to the parent RPC.
- The lambda will be executed directly in the receiving thread of the last response
  that comes in (this can be changed easily).


## Current State

- [x] protoc plugin
- [x] Client Unary-Unary calls
- [x] Server Unary-Unary handling
- [ ] Client Unary-stream calls
- [ ] Client Stream-Unary calls
- [ ] Client Stream-Stream calls
- [ ] Server Unary-stream handling
- [ ] Server Stream-Unary handling
- [ ] Server Stream-Stream handling

Next steps:
- Big cleanup and documentation pass.
- Tests, lots of tests.

## Examples:

Initializing the library and calling a method on a server.

```cpp
#include "easy_grpc/easy_grpc.h"
#include "generated/data.sgrpc.pb.h"

namespace rpc = easy_grpc;
namespace client = rpc::client;

int main() {
  // Library initialization.
  rpc::Environment grpc_env;

  // Completion queue + single handling thread
  rpc::Completion_queue cq;
  
  // Connection to server, with a default completion queue.
  client::Unsecure_channel channel("localhost:12345", &cq);

  // Connection to service.
  pkg::MyService::Stub stub(&channel);

  // Call method.
  auto done = stub.MyMethod({})
    .then([](auto rep) {
      std::cout << "MyMethod returned: " << rep.DebugString() << "\n";
    });


  // Wait until the result is done
  done.get();
  
  return 0;
}
```

Synchronously calling a method on the server: 

```cpp
int main() {
  // ...
  
  // This converts the rpc::Future<Reply> to a std::future<Reply>, and calls get() on it.
  auto rep = stub.MyMethod({}).get();
  
  return 0;
```

Chaining calls:

```cpp
int main() {
  // ...
  
  auto reply_3 = stub.MyMethod({})
    .then([&](auto rep) {
      return stub.MyMethod2({});
    })
    .then([&](auto rep2) {
      return stub.MyMethod3({});
    });
  
  return 0;
```


Waiting on multiple calls:

```cpp
int main() {
  //...

  auto reply_1 = stub.MyMethod({});
  auto reply_2 = stub.MyMethod2({});

  tie(reply_1, reply_2).then_finally([](auto rep_1, auto rep_2) {
    std::cerr << "1 and 2 are both done!\n";
  });


  return 0;
```

Server sending rpcs during handling:

```cpp
class MyService_impl : public pkg::MyService {
public:
  MyService_impl(pkg::MyService2::Stub_interface* stub) : stub_(stub) {}

  rpc::Future<pkg::Reply> MyMethod(pkg::Request) {
    pkg::MyRequest2 stub_request;

    return stub_->MyMethod2(stub_request)
      .then(auto sub_reply) {
        pkg::Reply result;
        return result;
      }
  }

private:
  pkg::MyService2::Stub_interface* stub_;
};
```
## Futures

    template<typename Ts...>
    class Future {
      // Calls callback with Ts... as arguments the future is ready.
      // - Errors are propagated to the resulting Future without invoking callback.
      // - If callback throws, the resulting Future will contain the throw exception
      // - The returned future will be fullfilled once callback returns successfully
      // - if callback returns a Future<...>, that future will be handed off to the result Future (i.e. no Future<Future<T>>)
      [[nodiscard]] Future<N/A | decltype(callback(Ts...))> then(callback);

      // Calls callback with Ts... as arguments the future is ready. 
      // - callback's return value is ignored.
      // - if callback throws, it will lead to a std::terminate()
      void then_finally(callback);

      // Calls callback with expected<Ts>... as arguments
      // - If callback throws, the resulting Future will contain the throw exception 
      // - callback will always be invoked, regardless of wether this future is fullfilled or failed. 
      // - The returned future will be fullfilled once callback returns successfully
      // - if callback returns a Future<...>, that future will be handed off to the result Future (i.e. no Future<Future<T>>)
      [[nodiscard]] Future<N/A | decltype(callback(Ts...))> then_expect(callback);


      // Calls callback with expected<Ts>... as arguments
      // - callback's return value is ignored.
      // - callback will always be invoked, regardless of wether this future is fullfilled or failed. 
      // - if callback throws, it will lead to a std::terminate()
      void then_finally_expect(callback);
    };

### Tieing futures

`tie()` can be used to create a future that completes when all passed futures are done.

    Future<int> a_fut;
    Future<bool> b_fut;

    auto done = tie(a, b).then([](int a, bool b) {});



## Design philosophy

* **Simple yet flexible:** The library imposes as few restrictions as possible.
* **Asynchronous by default:** In fact, the library only exposes asynchronous interfaces. 

