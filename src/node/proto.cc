#include "proto.h"

#include <iostream>
#ifdef __cplusplus
extern "C" {
#endif
#include "../core/proto.h"
#ifdef __cplusplus
}
#endif

namespace SgeProto {
using v8::Array;
using v8::ArrayBuffer;
using v8::Context;
using v8::Exception;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::Global;
using v8::Isolate;
using v8::Local;
using v8::Maybe;
using v8::MaybeLocal;
using v8::NewStringType;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::String;
using v8::Uint8Array;
using v8::Value;

thread_local Global<Function> Proto::constructor_from_content;
thread_local Global<Function> Proto::constructor_from_file;

// FIXME: 还没想到什么好办法，先这样
static thread_local char STR_CONTAINER[1024];

static int node_get_(const void* ud, const struct sge_key* k,
                     struct sge_value* v) {
  Object* obj = (Object*)ud;
  Isolate* isolate = obj->GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Value> key = String::NewFromUtf8(isolate, k->name.s).ToLocalChecked();

  Local<Value> value = obj->Get(context, key).ToLocalChecked();
  if (k->t == KT_LIST_INDEX) {
    value = value.As<Array>()->Get(context, k->idx).ToLocalChecked();
  }

  if (value.IsEmpty()) {
    sge_value_nil(v);
    return SGE_OK;
  }

  if (value->IsNumber()) {
    sge_value_integer(v, value->IntegerValue(context).ToChecked());
  } else if (value->IsString()) {
    Local<String> s = value->ToString(context).ToLocalChecked();
    size_t len = s->Utf8Length(isolate);
    s->WriteUtf8(isolate, STR_CONTAINER);
    sge_value_string_with_len(v, STR_CONTAINER, len);
  } else if (value->IsArray()) {
    Local<Array> arr = value.As<Array>();
    sge_value_integer(v, arr->Length());
  } else if (value->IsObject()) {
    Local<Object> o = value.As<Object>();
    sge_value_any(v, (void*)(*o));
  } else {
    sge_value_nil(v);
    return SGE_ERR;
  }

  return SGE_OK;
}

static void* node_set_(void* ud, const struct sge_key* k,
                       const struct sge_value* v) {
  Object* obj = (Object*)ud;
  Isolate* isolate = obj->GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();
  Local<Value> value;
  int t = sge_value_type(v->t);
  int is_arr = sge_is_list(v->t);

  if (is_arr) {
    value = Array::New(isolate, v->v.i);
  } else {
    switch (t) {
      case FIELD_TYPE_INTEGER:
        value = Number::New(isolate, v->v.i);
        break;

      case FIELD_TYPE_STRING:
        value = String::NewFromUtf8(isolate, v->v.s.s, NewStringType::kNormal,
                                    v->v.s.l)
                    .ToLocalChecked();
        break;

      case FIELD_TYPE_CUSTOM:
        value = Object::New(isolate);
        break;

      default:
        break;
    }
  }

  if (value.IsEmpty()) {
    return NULL;
  }

  if (k->t == KT_STRING) {
    Local<String> key = String::NewFromUtf8(isolate, k->name.s,
                                            NewStringType::kNormal, k->name.l)
                            .ToLocalChecked();
    Maybe<bool> r = obj->Set(context, key, value);
    sge_unused(r);
  } else if (k->t == KT_LIST_INDEX) {
    Array* arr = (Array*)ud;
    Maybe<bool> r = arr->Set(context, k->idx, value);
    sge_unused(r);
  }

  return (void*)*value;
}

static struct sge_proto* create_proto_(const FunctionCallbackInfo<Value>& args,
                                       enum ProtoParseType type) {
  int ret = 0;
  const char* err = NULL;
  struct sge_proto* proto = NULL;
  Isolate* isolate = args.GetIsolate();

  assert(type == ProtoParseType::PARSE_FROM_CONTENT ||
         type == ProtoParseType::PARSE_FROM_FILE);

  if (type == ProtoParseType::PARSE_FROM_FILE) {
    String::Utf8Value filenameObj(isolate, args[0]);
    char* filename = *filenameObj;
    proto = sge_parse(filename);

  } else if (type == ProtoParseType::PARSE_FROM_CONTENT) {
    String::Utf8Value textObj(isolate, args[0]);
    char* text = *textObj;
    proto = sge_parse_content(text, textObj.length());
  }

  if (proto == NULL) {
    isolate->ThrowException(Exception::Error(
        String::NewFromUtf8(isolate, "memory not enough").ToLocalChecked()));
  }

  ret = sge_proto_error(proto, &err);
  if (SGE_OK != ret) {
    isolate->ThrowException(
        Exception::Error(String::NewFromUtf8(isolate, err).ToLocalChecked()));
    sge_free_proto(proto);
    proto = NULL;
  }

  return proto;
}

void Proto::Init(v8::Isolate* isolate) {
  Local<FunctionTemplate> tplContent =
      FunctionTemplate::New(isolate, NewFromContent);
  Local<FunctionTemplate> tplFile = FunctionTemplate::New(isolate, NewFromFile);

  tplContent->SetClassName(
      String::NewFromUtf8(isolate, "Proto", NewStringType::kNormal)
          .ToLocalChecked());
  tplContent->InstanceTemplate()->SetInternalFieldCount(1);

  tplFile->SetClassName(
      String::NewFromUtf8(isolate, "Proto", NewStringType::kNormal)
          .ToLocalChecked());
  tplFile->InstanceTemplate()->SetInternalFieldCount(1);

  // 原型
  NODE_SET_PROTOTYPE_METHOD(tplContent, "encode", encode);
  NODE_SET_PROTOTYPE_METHOD(tplContent, "decode", decode);
  NODE_SET_PROTOTYPE_METHOD(tplContent, "debug", debug);
  NODE_SET_PROTOTYPE_METHOD(tplFile, "encode", encode);
  NODE_SET_PROTOTYPE_METHOD(tplFile, "decode", decode);
  NODE_SET_PROTOTYPE_METHOD(tplFile, "debug", debug);

  Local<Context> context = isolate->GetCurrentContext();
  constructor_from_content.Reset(
      isolate, tplContent->GetFunction(context).ToLocalChecked());
  constructor_from_file.Reset(isolate,
                              tplFile->GetFunction(context).ToLocalChecked());

  node::AddEnvironmentCleanupHook(
      isolate, [](void*) { constructor_from_content.Reset(); }, nullptr);
  node::AddEnvironmentCleanupHook(
      isolate, [](void*) { constructor_from_file.Reset(); }, nullptr);
}

void Proto::NewFromContent(const FunctionCallbackInfo<Value>& args) {
  struct sge_proto* proto = NULL;
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  if (args.IsConstructCall()) {
    proto = create_proto_(args, ProtoParseType::PARSE_FROM_CONTENT);
    if (NULL == proto) return;

    Proto* obj = new Proto(proto);
    Local<Object> warpObj = args.This();
    obj->Wrap(warpObj);
    args.GetReturnValue().Set(warpObj);
  } else {
    const int argc = 1;
    Local<Value> argv[argc] = {args[0]};
    Local<Function> cons =
        Local<Function>::New(isolate, constructor_from_content);
    Local<Object> instance =
        cons->NewInstance(context, argc, argv).ToLocalChecked();
    args.GetReturnValue().Set(instance);
  }
}

void Proto::NewFromFile(const FunctionCallbackInfo<Value>& args) {
  struct sge_proto* proto = NULL;
  Isolate* isolate = args.GetIsolate();
  Local<Context> context = isolate->GetCurrentContext();

  if (args.IsConstructCall()) {
    proto = create_proto_(args, ProtoParseType::PARSE_FROM_FILE);
    if (NULL == proto) return;

    Proto* obj = new Proto(proto);
    Local<Object> warpObj = args.This();
    obj->Wrap(warpObj);
    args.GetReturnValue().Set(warpObj);
  } else {
    const int argc = 1;
    Local<Value> argv[argc] = {args[0]};
    Local<Function> cons = Local<Function>::New(isolate, constructor_from_file);
    Local<Object> instance =
        cons->NewInstance(context, argc, argv).ToLocalChecked();
    args.GetReturnValue().Set(instance);
  }
}

void Proto::Create(const FunctionCallbackInfo<Value>& args,
                   enum ProtoParseType type) {
  const unsigned argc = 1;
  Isolate* isolate = args.GetIsolate();
  Local<Value> argv[argc] = {args[0]};

  assert(type == ProtoParseType::PARSE_FROM_CONTENT ||
         type == ProtoParseType::PARSE_FROM_FILE);

  if (type == ProtoParseType::PARSE_FROM_CONTENT) {
    Local<Function> cons =
        Local<Function>::New(isolate, constructor_from_content);
    Local<Context> context = isolate->GetCurrentContext();
    MaybeLocal<Object> mInstance = cons->NewInstance(context, argc, argv);
    if (!mInstance.IsEmpty()) {
      Local<Object> instance = mInstance.ToLocalChecked();
      args.GetReturnValue().Set(instance);
    }
  } else if (type == ProtoParseType::PARSE_FROM_FILE) {
    Local<Function> cons = Local<Function>::New(isolate, constructor_from_file);
    Local<Context> context = isolate->GetCurrentContext();
    MaybeLocal<Object> mInstance = cons->NewInstance(context, argc, argv);
    if (!mInstance.IsEmpty()) {
      Local<Object> instance = mInstance.ToLocalChecked();
      args.GetReturnValue().Set(instance);
    }
  }
}

Proto::~Proto() { sge_free_proto((struct sge_proto*)this->private_data_); }

void Proto::encode(const FunctionCallbackInfo<Value>& args) {
  int argc = 0;
  size_t err_msg_len = 0;
  struct sge_string* result = NULL;
  char error_msg[1024];
  char proto_name[512];
  Isolate* isolate = args.GetIsolate();
  Proto* proto_obj = ObjectWrap::Unwrap<Proto>(args.Holder());
  struct sge_proto* proto = (struct sge_proto*)proto_obj->private_data_;

  argc = args.Length();
  if (argc != 2) {
    err_msg_len =
        std::snprintf(error_msg, 1024, "expect 2 arguments, but got %d", argc);
    error_msg[err_msg_len] = '\0';
    isolate->ThrowException(Exception::Error(
        String::NewFromUtf8(isolate, error_msg).ToLocalChecked()));
    return;
  }

  if (!args[0]->IsString()) {
    Local<String> type = args[0]->TypeOf(isolate);
    err_msg_len = std::snprintf(error_msg, 1024,
                                "except argument 1 is string type, but got ");
    type->WriteUtf8(isolate, error_msg + err_msg_len);
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, error_msg).ToLocalChecked()));
    return;
  }

  if (!args[1]->IsObject()) {
    Local<String> type = args[1]->TypeOf(isolate);
    err_msg_len = std::snprintf(error_msg, 1024,
                                "except argument 2 is object type, but got ");
    type->WriteUtf8(isolate, error_msg + err_msg_len);
    isolate->ThrowException(Exception::TypeError(
        String::NewFromUtf8(isolate, error_msg).ToLocalChecked()));
    return;
  }

  args[0]
      ->ToString(isolate->GetCurrentContext())
      .ToLocalChecked()
      ->WriteUtf8(isolate, proto_name);
  Local<Object> ud =
      args[1]->ToObject(isolate->GetCurrentContext()).ToLocalChecked();
  result = sge_encode(proto, proto_name, (const void*)(*ud), node_get_);
  if (NULL == result) {
    const char* err = NULL;
    sge_proto_error(proto, &err);
    isolate->ThrowException(
        Exception::Error(String::NewFromUtf8(isolate, err).ToLocalChecked()));
    return;
  }

  Local<ArrayBuffer> buffer = ArrayBuffer::New(isolate, result->l);
  memcpy((char*)buffer->Data(), result->s, result->l);

  Local<Uint8Array> ret = Uint8Array::New(buffer, 0, result->l);
  sge_free_string(result);

  args.GetReturnValue().Set(ret);
}

void Proto::decode(const FunctionCallbackInfo<Value>& args) {
  int argc = 0;
  size_t err_msg_len = 0;
  char error_msg[1024];
  Isolate* isolate = args.GetIsolate();

  argc = args.Length();
  if (argc != 1) {
    err_msg_len =
        std::snprintf(error_msg, 1024, "expect 1 arguments, but got %d", argc);
    error_msg[err_msg_len] = '\0';
    isolate->ThrowException(Exception::Error(
        String::NewFromUtf8(isolate, error_msg).ToLocalChecked()));
    return;
  }

  if (!args[0]->IsUint8Array()) {
    err_msg_len =
        std::snprintf(error_msg, 1024, "expect 1 arguments, but got %d", argc);
    Local<String> type = args[0]->TypeOf(isolate);
    type->WriteUtf8(isolate, error_msg + err_msg_len);
    isolate->ThrowException(Exception::Error(
        String::NewFromUtf8(isolate, error_msg).ToLocalChecked()));
    return;
  }

  Local<Uint8Array> u8_arr = args[0].As<Uint8Array>();
  Local<ArrayBuffer> arr = u8_arr->Buffer();
  size_t arr_len = arr->ByteLength();
  const char* buffer = (const char*)arr->Data();
  Local<Object> out = Object::New(isolate);
  Proto* proto_obj = ObjectWrap::Unwrap<Proto>(args.Holder());
  struct sge_proto* proto = (struct sge_proto*)proto_obj->private_data_;
  int ret = 0;

  ret = sge_decode(proto, buffer, arr_len, (void*)*out, node_set_);
  if (SGE_OK != ret) {
    const char* err = NULL;
    sge_proto_error(proto, &err);
    isolate->ThrowException(
        Exception::Error(String::NewFromUtf8(isolate, err).ToLocalChecked()));
    return;
  }

  args.GetReturnValue().Set(out);
}

void Proto::debug(const FunctionCallbackInfo<Value>& args) {
  Proto* proto_obj = ObjectWrap::Unwrap<Proto>(args.Holder());
  struct sge_proto* proto = (struct sge_proto*)proto_obj->private_data_;

  sge_print_proto(proto);
}

}  // namespace SgeProto
