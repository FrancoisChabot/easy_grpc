# Implementing Services

## From proto file description

### Generate the service code

Using the plugin built as part of this project, you can have `protoc` generate the headers and source automatically, 
much like with regular grpc.

`protoc --egrpc_out=path/to/source/dst --plugin=protoc-gen-egrpc=path/to/easy_grpc_protoc_plugin my_service.proto`

### Writing the service implementation

Services are *duck-typed*. That is, your implementation does not need to inherit from the service class itself. 
It just has to fullfill the expected interface. The only important function in the generated code is `My_service::get_config()`
which binds an object as a service implementation, and validates that the implementation is complete.

So, given the following proto file:

```protobuf
syntax = "proto3";
package pkg;

message HelloRequest {
  string name = 1;
}

message HelloReply {
  string greeting = 1;
}

service HelloService {
  rpc SayHello(HelloRequest) returns (HelloReply) {}
  rpc SayBye(HelloRequest) returns (HelloReply) {}
}
```

An implementation could look like this:

```cpp
class Hello_impl {
public:
  // Says hello
  pkg::HelloReply SayHello(const pkg::HelloRequest& req) {
    return {};
  }

  // Says Goodbye, asynchronously
  easy_grpc::Future<pkg::HelloReply> SayBye(const pkg::HelloRequest& req) {
    return pkg::HelloReply{};
  }
};
```

Notice how the methods can return either `return_type` or `easy_grpc::Future<return_type`. This denotes the difference between
synchronous and asynchronous handling.


### Using the service implementation

```cpp
namespace rpc = easy_grpc;
int main() {

  My_service_impl impl;

  rpc::server::Config server_config;

  server_config.add_service(MyService::get_config(service))
  ...
}
```

## From scratch

If your data format is not defined as protocol buffers, then you will have to bypass the code generation
and define the service yourself.

### Serializing the messages.

**This part of the process is not nearly as straightforward as it could be, and will be improved**

Whichever types you will be using for rpc arguments and return types will need to be serializable.
This is done by specializing `::easy_grpc::Serializer<T>` like so:

```cpp
namespace easy_grpc {
template<>
struct Serializer<My_message_type> {
  static grpc_byte_buffer* serialize(const My_message_type& msg) {
    return ...;
  }

  static My_message_type deserialize(grpc_byte_buffer* data) {
    return ...;
  }
};
```

(see the code in `examples/01_non_protobuf/packet.h` for a full example).

### Implementing the service.

The service per-se is defined by an instance of `easy_grpc::Service_config`. Simply attach method handlers
to the service by using `add_method()`. 

`add_method()` will deduce the sync/async and streaming/unary nature of the functions
from the arguments of the passed handlers.

```cpp
namespace rpc = easy_grpc;

Reply_packet foo_handler(Request_packet req) {
  return {};
}

Future<Reply_packet> bar_handler(Request_packet req) {
  return Reply_packet{};
}

int main() {
  rpc::server::Service_config service_cfg("test.My_service");

  service_cfg.add_method("foo", foo_handler);
  service_cfg.add_method("bar", bar_handler);


  rpc::server::Config server_config;

  server_config.add_service(std::move(service_cfg));
  ...
}
```

The following pattern can also be used to easily make services that look very much like proto-generated
OOP services:

```cpp
class My_service {
public:
  Reply_packet foo(Request_packet req) {
    return {};
  }

  Future<Reply_packet> bar(Request_packet req) {
    return Reply_packet{};
  }

  rpc::server::Service_config make_config() {
    rpc::server::Service_config my_service_cfg("test.My_service");

    my_service_cfg.add_method("foo", [this](Request_packet req){return foo(std::move(req));});
    my_service_cfg.add_method("bar", [this](Request_packet req){return bar(std::move(req));});

    return my_service_cfg;
  }
};
```

## Cheat sheets:

Service syntax:

```cpp
namespace rpc = easy_grpc;
Service_impl {
public:
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
};
```

Server reader:

