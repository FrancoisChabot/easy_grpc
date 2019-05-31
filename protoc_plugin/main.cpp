#include <google/protobuf/compiler/code_generator.h>
#include <google/protobuf/compiler/plugin.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>

#include <filesystem>
#include <functional>

using google::protobuf::Descriptor;
using google::protobuf::FileDescriptor;
using google::protobuf::ServiceDescriptor;

using google::protobuf::compiler::CodeGenerator;
using google::protobuf::compiler::GeneratorContext;
using google::protobuf::io::CodedOutputStream;
using google::protobuf::io::ZeroCopyOutputStream;

using std::filesystem::path;

const char* header_extension = ".egrpc.pb.h";
const char* source_extension = ".egrpc.pb.cc";
const char* pb_header_extension = ".pb.h";

std::string class_name(const Descriptor* desc) {
  auto outer = desc;
  while (outer->containing_type()) {
    outer = outer->containing_type();
  }

  auto outer_name = outer->full_name();
  auto inner_name = desc->full_name().substr(outer_name.size());

  std::ostringstream result;
  result << "::";

  for (auto& c : outer_name) {
    if (c == '.') {
      result << "::";
    } else {
      result << c;
    }
  }

  for (auto& c : inner_name) {
    if (c == '.') {
      result << "_";
    } else {
      result << c;
    }
  }

  return result.str();
}

std::string header_guard(std::string_view file_name) {
  std::ostringstream result;
  for (auto c : file_name) {
    if (std::isalnum(c)) {
      result << c;
    } else {
      result << std::hex << int(c);
    }
  }
  return result.str();
}

std::vector<std::string> tokenize(const std::string& str, char c) {
  std::vector<std::string> result;
  if (!str.empty()) {
    auto current = 0;
    auto next = str.find(c);

    while (next != std::string::npos) {
      result.push_back(str.substr(current, next));
      current = next + 1;
      next = str.find(c, current);
    }

    if (current != next) {
      result.push_back(str.substr(current, next));
    }
  }

  return result;
}

std::vector<std::string> package_parts(const FileDescriptor* file) {
  return tokenize(file->package(), '.');
}

void generate_service_header(const ServiceDescriptor* service,
                             std::ostream& dst) {
 
  auto name = service->name();

  auto method_name_cste = [&](auto method) {
    return std::string("k") + name + "_" + method->name() + "_name";
  };

  dst << "class " << service->name() << " {\n"
      << "public:\n"
      << "  using service_type = " << name << ";\n\n"
      << "  virtual ~" << name << "() {}\n\n";


  for (int i = 0; i < service->method_count(); ++i) {
    auto method = service->method(i);

    dst << "  static const char * " << method_name_cste(method) << ";\n";
  }
  dst << "\n";

  for (int i = 0; i < service->method_count(); ++i) {
    auto method = service->method(i);

    auto input = method->input_type();
    auto output = method->output_type();

    dst << "  virtual ::easy_grpc::Future<" << class_name(output) << "> "
        << method->name() << "(const " << class_name(input) << "&) = 0;\n";
  }
  dst << "\n";

  dst << "  class Stub_interface {\n"
      << "  public:\n"
      << "    virtual ~Stub_interface() {}\n\n";

  for (int i = 0; i < service->method_count(); ++i) {
    auto method = service->method(i);

    auto input = method->input_type();
    auto output = method->output_type();

    dst << "    virtual ::easy_grpc::Future<" << class_name(output) << "> "
        << method->name() << "(" << class_name(input)
        << ", ::easy_grpc::client::Call_options={}) = 0;\n";
  }

  dst << "  };\n\n";
  dst << "  class Stub final : public Stub_interface {\n"
      << "  public:\n"
      << "    Stub(::easy_grpc::client::Channel*, "
         "::easy_grpc::Completion_queue* = nullptr);\n\n";

  for (int i = 0; i < service->method_count(); ++i) {
    auto method = service->method(i);

    auto input = method->input_type();
    auto output = method->output_type();

    dst << "    ::easy_grpc::Future<" << class_name(output) << "> "
        << method->name() << "(" << class_name(input)
        << ", ::easy_grpc::client::Call_options={}) override;\n";
  }

  dst << "\n"
      << "  private:\n"
      << "    ::easy_grpc::client::Channel* channel_;\n"
      << "    ::easy_grpc::Completion_queue* default_queue_;\n\n";

  for (int i = 0; i < service->method_count(); ++i) {
    auto method = service->method(i);
    dst << "    void* " << method->name() << "_tag_;\n";
  }
  dst << "  };\n\n";

  dst << "  template<typename ImplT>\n"
      << "  static ::easy_grpc::server::Service_config get_config(ImplT& impl) {\n"
      << "    ::easy_grpc::server::Service_config result(\""<< name <<"\");\n\n";
  for (int i = 0; i < service->method_count(); ++i) {
    auto method = service->method(i);
    auto input = method->input_type();
    auto output = method->output_type();

    dst << "    result.add_method("
        << method_name_cste(method) << ", [&impl](" << class_name(input) << " req){return impl." << method->name() << "(std::move(req));});\n"; 
  }

  dst << "\n    return result;\n";
  dst << "  }\n";
  dst << "};\n\n";
}

void generate_service_source(const ServiceDescriptor* service,
                             std::ostream& dst) {
  auto name = service->name();

  dst << "// ********** " << name << " ********** //\n\n";

  auto method_name_cste = [&](auto method) {
    return std::string("k") + name + "_" + method->name() + "_name";
  };

  for (int i = 0; i < service->method_count(); ++i) {
    auto method = service->method(i);
    dst << "const char* " << name << "::" << method_name_cste(method) << " = \"/"
        << service->file()->package() << "." << name << "/" << method->name()
        << "\";\n";
  }
  
  dst << "\n";

  dst << name
      << "::Stub::Stub(::easy_grpc::client::Channel* c, "
         "::easy_grpc::Completion_queue* default_queue)\n"
      << "  : channel_(c), default_queue_(default_queue ? default_queue : "
         "c->default_queue())";
  for (int i = 0; i < service->method_count(); ++i) {
    auto method = service->method(i);
    dst << "\n  , " << method->name() << "_tag_(c->register_method("
        << method_name_cste(method) << "))";
  }
  dst << " {}\n\n";

  for (int i = 0; i < service->method_count(); ++i) {
    auto method = service->method(i);

    auto input = method->input_type();
    auto output = method->output_type();

    dst << "// " << method->name() << "\n";
    dst << "::easy_grpc::Future<" << class_name(output) << "> " << name
        << "::Stub::" << method->name() << "(" << class_name(input)
        << " req, ::easy_grpc::client::Call_options options) {\n"
        << "  if(!options.completion_queue) { options.completion_queue = "
           "default_queue_; }\n"
        << "  return ::easy_grpc::client::start_unary_call<"
        << class_name(output) << ">(channel_, " << method->name()
        << "_tag_, std::move(req), std::move(options));\n"
        << "};\n\n";
  }

}

std::string generate_header(const FileDescriptor* file) {
  std::ostringstream result;

  auto file_name = path(file->name()).stem().string();

  // Prologue
  result << "// This code was generated by the easy_grpc protoc plugin.\n"
         << "#ifndef EASY_GRPC_" << header_guard(file_name) << "_INCLUDED_H\n"
         << "#define EASY_GRPC_" << header_guard(file_name) << "_INCLUDED_H\n"
         << "\n";

  // Headers
  result << "#include \"" << file_name << pb_header_extension << "\""
         << "\n\n";

  result << "#include \"easy_grpc/ext_protobuf/gen_support.h\""
         << "\n\n";

  result << "#include <memory>\n\n";

  // namespace
  auto package = package_parts(file);

  if (!package.empty()) {
    for (const auto& p : package) {
      result << "namespace " << p << " {\n";
    }
    result << "\n";
  }

  // Services
  for (int i = 0; i < file->service_count(); ++i) {
    generate_service_header(file->service(i), result);
  }

  // Epilogue
  if (!package.empty()) {
    for (const auto& p : package) {
      result << "} // namespace" << p << "\n";
    }
    result << "\n";
  }

  result << "#endif\n";

  return result.str();
}

std::string generate_source(const FileDescriptor* file) {
  std::ostringstream result;

  auto file_name = path(file->name()).stem().string();

  // Prologue
  result << "// This code was generated by the easy_grpc protoc plugin.\n\n";

  // Headers
  result << "#include \"" << file_name << header_extension << "\""
         << "\n\n";

  // namespace
  auto package = package_parts(file);

  if (!package.empty()) {
    for (const auto& p : package) {
      result << "namespace " << p << " {\n";
    }
    result << "\n";
  }

  // Services
  for (int i = 0; i < file->service_count(); ++i) {
    generate_service_source(file->service(i), result);
  }

  // Epilogue
  if (!package.empty()) {
    for (const auto& p : package) {
      result << "} // namespace" << p << "\n";
    }
    result << "\n";
  }

  return result.str();
}

class Generator : public CodeGenerator {
 public:
  // Generates code for the given proto file, generating one or more files in
  // the given output directory.
  //
  // A parameter to be passed to the generator can be specified on the command
  // line. This is intended to be used to pass generator specific parameters.
  // It is empty if no parameter was given. ParseGeneratorParameter (below),
  // can be used to accept multiple parameters within the single parameter
  // command line flag.
  //
  // Returns true if successful.  Otherwise, sets *error to a description of
  // the problem (e.g. "invalid parameter") and returns false.
  bool Generate(const FileDescriptor* file, const std::string& parameter,
                GeneratorContext* context, std::string* error) const override {
    try {
      auto file_name = path(file->name()).stem().string();

      {
        auto header_data = generate_header(file);

        std::unique_ptr<ZeroCopyOutputStream> header_dst{
            context->Open(file_name + header_extension)};
        CodedOutputStream header_out(header_dst.get());
        header_out.WriteRaw(header_data.data(), header_data.size());
      }

      {
        auto src_data = generate_source(file);
        std::unique_ptr<ZeroCopyOutputStream> src_dst{
            context->Open(file_name + source_extension)};
        CodedOutputStream src_out(src_dst.get());
        src_out.WriteRaw(src_data.data(), src_data.size());
      }
    } catch (std::exception& e) {
      *error = e.what();
      return false;
    }

    return true;
  }
};

int main(int argc, char* argv[]) {
  Generator generator;
  ::google::protobuf::compiler::PluginMain(argc, argv, &generator);
}