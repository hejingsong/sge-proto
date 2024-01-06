#include <node.h>
#include <node_object_wrap.h>

namespace SgeProto {
enum ProtoParseType { PARSE_FROM_CONTENT = 1, PARSE_FROM_FILE = 2 };

class Proto : public node::ObjectWrap {
 public:
  static void Init(v8::Isolate* isolate);
  static void Create(const v8::FunctionCallbackInfo<v8::Value>& args, enum ProtoParseType type);

 private:
  static void NewFromContent(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void NewFromFile(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void encode(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void decode(const v8::FunctionCallbackInfo<v8::Value>& args);
  static void debug(const v8::FunctionCallbackInfo<v8::Value>& args);

 private:
  explicit Proto(void* private_data): private_data_(private_data) {}
  virtual ~Proto();

 private:
  void* private_data_;
  thread_local static v8::Global<v8::Function> constructor_from_content;
  thread_local static v8::Global<v8::Function> constructor_from_file;
};
}  // namespace SgeProto
