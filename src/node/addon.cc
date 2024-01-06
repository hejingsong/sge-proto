#include <node.h>
#include <node_object_wrap.h>
#include <v8.h>

#include "proto.h"

namespace SgeProto {
using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::Exception;
using v8::FunctionCallbackInfo;
using v8::Isolate;
using v8::Local;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::String;
using v8::Uint8Array;
using v8::Value;

void parse(const FunctionCallbackInfo<Value> &args) {
  Proto::Create(args, ProtoParseType::PARSE_FROM_CONTENT);
}

void parse_file(const FunctionCallbackInfo<Value> &args) {
  Proto::Create(args, ProtoParseType::PARSE_FROM_FILE);
}

void Initialize(v8::Local<v8::Object> exports, v8::Local<v8::Value> module,
                void *priv) {
  Proto::Init(exports->GetIsolate());
  NODE_SET_METHOD(exports, "parse", parse);
  NODE_SET_METHOD(exports, "parse_file", parse_file);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

}  // namespace SgeProto
