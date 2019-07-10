#include "easy_grpc_reflection/reflection.h"
#include "easy_grpc_reflection/generated/reflection.egrpc.pb.h"

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>


using grpc::reflection::v1alpha::ServerReflectionRequest;
using grpc::reflection::v1alpha::ServerReflectionResponse;
using grpc::reflection::v1alpha::ExtensionRequest;

namespace easy_grpc {
class Reflection_impl {
public:

  Reflection_impl(std::vector<std::string> services) 
    : services_(std::move(services))
    , descriptor_pool_(google::protobuf::DescriptorPool::generated_pool()) {
  }

  using service_type = ::grpc::reflection::v1alpha::ServerReflection;

  Stream_future<ServerReflectionResponse> ServerReflectionInfo(Stream_future<ServerReflectionRequest> req_stream) {
    auto prom = std::make_shared<Stream_promise<ServerReflectionResponse>>() ;
    Stream_future<ServerReflectionResponse> result = prom->get_future();

    req_stream.for_each([this, prom](ServerReflectionRequest req){
      std::cout << req.DebugString() << "\n";

      try {
        switch(req.message_request_case()) {
          case ServerReflectionRequest::MessageRequestCase::kFileByFilename:
            prom->push(get_file_by_name(req.file_by_filename()));
            break;
          case ServerReflectionRequest::MessageRequestCase::kFileContainingSymbol:
            prom->push(get_file_containing_symbol(req.file_containing_symbol()));
            break;
          case ServerReflectionRequest::MessageRequestCase::kFileContainingExtension:
            prom->push(get_file_containing_extension(req.file_containing_extension()));
            break;
          case ServerReflectionRequest::MessageRequestCase::kAllExtensionNumbersOfType:
            prom->push(get_all_extension_numbers(req.all_extension_numbers_of_type()));
            break;
          case ServerReflectionRequest::MessageRequestCase::kListServices:
            prom->push(list_services());
            break;
          default:
            throw error::unimplemented("reflection method missing");
            break;
        }
      }
      catch(...) {
        prom->set_exception(std::current_exception());
      }
    }).finally([prom](expected<void>){
      prom->complete();
    });

    return result;
  }

private:
std::vector<std::string> services_;
const google::protobuf::DescriptorPool* descriptor_pool_ = nullptr;

ServerReflectionResponse list_services() {
  ServerReflectionResponse raw_rep;
  auto rep = raw_rep.mutable_list_services_response();

  for(const auto& s : services_) {
    rep->add_service()->set_name(s);
  }

  rep->add_service()->set_name("grpc.reflection.v1alpha.ServerReflection");

  return raw_rep;
}

ServerReflectionResponse get_file_by_name(const std::string& filename) {
  if(!descriptor_pool_) {
    throw error::cancelled("no descriptor info available");
  }

  auto file_desc = descriptor_pool_->FindFileByName(filename);

  if(!file_desc) {
    throw error::not_found("File not found");
  }

  ServerReflectionResponse raw_rep;
  std::unordered_set<std::string> seen_files;
  fill_file_response(file_desc, raw_rep, &seen_files);

  return raw_rep;
}

ServerReflectionResponse get_file_containing_symbol(const std::string& symbol) {
  if(!descriptor_pool_) {
    throw error::cancelled("no descriptor info available");
  }

  auto file_desc = descriptor_pool_->FindFileContainingSymbol(symbol);
  if(!file_desc) {
    throw error::not_found("Symbol not found");
  }

  ServerReflectionResponse raw_rep;
  std::unordered_set<std::string> seen_files;
  fill_file_response(file_desc, raw_rep, &seen_files);

  return raw_rep;
}

ServerReflectionResponse get_file_containing_extension(const ExtensionRequest& request) {
  if(!descriptor_pool_) {
    throw error::cancelled("no descriptor info available");
  }

  auto desc = descriptor_pool_->FindMessageTypeByName(request.containing_type());
  if(!desc) {
    throw error::not_found("Type not found");
  }

  auto field_desc = descriptor_pool_->FindExtensionByNumber(desc, request.extension_number());
  if(!field_desc) {
    throw error::not_found("Extension not found");
  }

  ServerReflectionResponse raw_rep;
  std::unordered_set<std::string> seen_files;
  fill_file_response(field_desc->file(), raw_rep, &seen_files);

  return raw_rep;
}

ServerReflectionResponse get_all_extension_numbers(const std::string& type) {
  if(!descriptor_pool_) {
    throw error::cancelled("no descriptor info available");
  }

  auto desc = descriptor_pool_->FindMessageTypeByName(type);
  if (desc == nullptr) {
    throw error::not_found("Type not found");
  }

  ServerReflectionResponse raw_rep;
  auto rep = raw_rep.mutable_all_extension_numbers_response();
  std::vector<const google::protobuf::FieldDescriptor*> extensions;
  descriptor_pool_->FindAllExtensions(desc, &extensions);
  for (auto it = extensions.begin(); it != extensions.end(); it++) {
    rep->add_extension_number((*it)->number());
  }
  rep->set_base_type_name(type);

  return raw_rep;
}


void fill_file_response(
  const google::protobuf::FileDescriptor* file_desc,
  ServerReflectionResponse& response,
  std::unordered_set<std::string>* seen_files) {
    if (seen_files->find(file_desc->name()) != seen_files->end()) {
      return;
    }
    seen_files->insert(file_desc->name());

    google::protobuf::FileDescriptorProto file_desc_proto;
    std::string data;
    file_desc->CopyTo(&file_desc_proto);
    file_desc_proto.SerializeToString(&data);
    response.mutable_file_descriptor_response()->add_file_descriptor_proto(data);

    for (int i = 0; i < file_desc->dependency_count(); ++i) {
      fill_file_response(file_desc->dependency(i), response, seen_files);
    }
  }
};

Reflection_feature::Reflection_feature() {}
Reflection_feature::Reflection_feature(Reflection_feature&& rhs) : impl_(std::move(rhs.impl_)) {}
Reflection_feature& Reflection_feature::operator=(Reflection_feature&& rhs) {
  impl_ = std::move(rhs.impl_);
  return *this;
}

Reflection_feature::~Reflection_feature() {}

void Reflection_feature::add_to_config(server::Config& cfg) {
    std::vector<std::string> service_names;

    for(const auto& s : cfg.get_services()) {
      service_names.push_back(s.name());
    }

    impl_ = std::make_unique<Reflection_impl>(std::move(service_names));

    cfg.add_service(*impl_);
}

}