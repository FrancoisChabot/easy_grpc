## Service 

### Rpc handler

```cpp
namespace rpc = easy_grpc;
// Unary Rpcs, like:
// rpc foo(Request) returns (Reply) {}
//
Reply foo(Request req, rpc::Server_context ctx);              // Synchronous handling
rpc::Future<Reply> foo(Request req, rpc::Server_context ctx); // Asynchronous handling

// Client streaming Rpcs, like:
// rpc foo(stream Request) returns (Reply) {}
//
Reply foo(rpc::Server_reader<Request> req_stream, rpc::Server_context ctx);              // Synchronous handling
rpc::Future<Reply> foo(rpc::Server_reader<Request> req_stream, rpc::Server_context ctx); // Asynchronous handling

// Server streaming Rpcs, like:
// rpc foo(Request) returns (stream Reply) {}
//
void foo(Request req, rpc::Server_writer rep_stream, rpc::Server_context ctx); // Always Asynchronous

// Bidirectional streaming Rpcs, like:
// rpc foo(stream Request) returns (stream Reply) {}
//
void foo(rpc::Server_reader<Request> req_stream, rpc::Server_writer rep_stream, rpc::Server_context ctx); // Always Asynchronous
```

### Server_reader<T>

```cpp
template<typename ReqT>
struct Server_reader {
  // Executes cb on each incoming message directly in the handling thread, and fullfills the returned future
  // once the last message has been handled.
  //
  // - If cb throws an exception, the returned future will be immediately failed
  // - If the rpc is cancelled, or otherwise fails, the returned future will be immediately failed
  template<typename CbT>
  rpc::Future<> for_each(CbT cb);

  // Queues an execution of cb in queue for each recieved message, and fullfills the returned future
  // once the last message has been handled.
  //
  // - There is no guarantee that queue.push() will be invoked once for each message, they could get batched.
  // - If cb throws an exception, the returned future will be immediately failed
  // - If the rpc is cancelled, or otherwise fails, the returned future will be immediately failed
  template<typename CbT, typename QueueT>
  rpc::Future<> for_each(CbT cb, QueueT& queue);
};
```

### Server_writer<T>

```cpp
template<typename RepT>
struct Server_writer {
  // appends a message to the stream of responses.
  void push(RepT&& data);

  // Terminate the rpc successfully.
  void finish();

  // Terminate the rpc with an error.
  template<typename ErrT>
  void fail(ErrT&& error);
};

```