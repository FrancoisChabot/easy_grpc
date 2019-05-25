# easy_grpc

A future-based grpc API for C++.

This project is an attempt to create a wrapper around GRPC to make
writing asynchronous servers and clients as straightforward as possible. It may not
be quite as optimal as a fully-customized server working on raw completion 
queues, but every effort is being made to get as close as possible while
maintaining as sane and modern an API as possible.

## Current State

- [x] Client Unary-Unary calls
- [ ] Client Unary-stream calls
- [ ] Client Stream-Unary calls
- [ ] Client Stream-Stream calls
- [ ] Server Unary-Unary handling
- [ ] Server Unary-stream handling
- [ ] Server Stream-Unary handling
- [ ] Server Stream-Stream handling


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

  // Completion queue + handling thread
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
  done.wait();
  
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

