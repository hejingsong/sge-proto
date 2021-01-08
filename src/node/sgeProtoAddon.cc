#include <v8.h>
#include <node.h>

#ifdef __cplusplus
extern "C"
{
#endif
#include "../core/sge_proto.h"
#ifdef __cplusplus
}
#endif

namespace sgeProto
{

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

static const uint16_t BUFFER_SIZE = 2048;
static void* g_memPtr = NULL;

static void getData(const void *object, sge_value *ud)
{
	Object *obj = (Object *)object;
	Isolate *isolate = obj->GetIsolate();
	Local<Context> context = isolate->GetCurrentContext();
	const char *fieldName = ud->name;
	Local<Value> key = String::NewFromUtf8(isolate, fieldName, NewStringType::kNormal).ToLocalChecked();
	Local<Value> value;

	if (ud->idx == -1)
	{
		value = obj->Get(context, key).ToLocalChecked();
	}
	else
	{
		value = obj->Get(context, ud->idx).ToLocalChecked();
	}

	if (value.IsEmpty())
	{
		return;
	}

	if (value->IsNumber())
	{
		*((long *)ud->ptr) = value->IntegerValue(context).ToChecked();
	}
	else if (value->IsString())
	{
		String *s = (String *)*value;
		String::Utf8Value v8Value(isolate, value);
		ud->len = s->Utf8Length(isolate);
		memcpy(g_memPtr, *v8Value, ud->len);
		ud->ptr = g_memPtr;
	}
	else if (value->IsArray())
	{
		Local<Array> arr = value.As<Array>();
		ud->ptr = *arr;
		ud->len = arr->Length();
	}
	else if (value->IsObject())
	{
		Local<Object> oValue = value.As<Object>();
		Isolate *oIsolate = oValue->GetIsolate();
		Local<Context> oContext = oIsolate->GetCurrentContext();
		Local<Array> keys = oValue->GetOwnPropertyNames(oContext).ToLocalChecked();
		ud->len = keys->Length();
		ud->ptr = *oValue;
	}
}

static void *setData(void *object, sge_value *ud)
{
	Object *obj = (Object *)object;
	Isolate *isolate = obj->GetIsolate();
	Local<Context> context = isolate->GetCurrentContext();
	Local<Value> value, key;

	switch (ud->vt)
	{
	case SGE_NUMBER:
		value = Number::New(isolate, *((long *)ud->ptr));
		break;
	case SGE_STRING:
		value = String::NewFromUtf8(isolate, (const char *)ud->ptr, NewStringType::kNormal, ud->len).ToLocalChecked();
		break;
	case SGE_LIST:
		value = Array::New(isolate, ud->len);
		break;
	case SGE_DICT:
		value = Object::New(isolate);
		break;
	default:
		break;
	}

	if (ud->idx == -1)
	{
		key = String::NewFromUtf8(isolate, ud->name, NewStringType::kNormal).ToLocalChecked();
		obj->Set(context, key, value);
	}
	else
	{
		Array *arr = (Array *)object;
		arr->Set(context, ud->idx, value);
	}

	return (void *)*value;
}

void parse(const FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();

	if (args.Length() < 1)
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument 1 must be string",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	if (!args[0]->IsString())
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument 1 must be string",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	String::Utf8Value textObj(isolate, args[0]);
	char *text = *textObj;

	if (sge_parse(text) != SGE_OK)
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"parse protocol fail.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}
}

void parseFile(const FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();

	if (args.Length() < 1)
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument 1 must be string",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	if (!args[0]->IsString())
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument 1 must be string",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	String::Utf8Value fileNameObj(isolate, args[0]);
	const char *fileName = *fileNameObj;
	if (sge_parse_file(fileName) != SGE_OK)
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"parse file fail.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}
}

void encode(const FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();
	Local<Context> context = isolate->GetCurrentContext();

	if (args.Length() < 2)
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument error.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	if (!args[0]->IsString())
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument 1 must be string.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	if (!args[1]->IsObject())
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument 1 must be object.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	String::Utf8Value protoNameObj(isolate, args[0]);
	Local<Object> userStruct = args[1]->ToObject(context).ToLocalChecked();
	Local<ArrayBuffer> buffer = ArrayBuffer::New(isolate, BUFFER_SIZE);
	int len = 0;
	const char *protoName = *protoNameObj;
	const void *userData = (const void *)*userStruct;
	char *pBuffer = (char *)buffer->GetContents().Data();
	len = sge_encode(protoName, userData, pBuffer, getData);
	if (len < 0)
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"encode fail.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}
	Local<Uint8Array> u8Arr = Uint8Array::New(buffer, 0, len);
	args.GetReturnValue().Set(u8Arr);
}

void decode(const FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();

	if (args.Length() < 1)
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument error.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	if (!args[0]->IsUint8Array())
	{
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument 1 must be ArrayBuffer.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	Local<Context> context = isolate->GetCurrentContext();
	Local<Uint8Array> u8Arr = args[0].As<Uint8Array>();
	Local<ArrayBuffer> arr = u8Arr->Buffer();
	Local<Object> obj = Object::New(isolate);
	const char *buffer = (const char *)arr->GetContents().Data();
	void *userData = (void *)*obj;
	int protoIdx = sge_decode(buffer, userData, setData);
	Local<Array> ret = Array::New(isolate, 2);
	Local<Number> protoIdxObj = Number::New(isolate, protoIdx);
	ret->Set(context, Number::New(isolate, 0), protoIdxObj);
	ret->Set(context, Number::New(isolate, 1), obj);

	args.GetReturnValue().Set(ret);
}

void destroy(const FunctionCallbackInfo<Value> &args)
{
	sge_destroy(1);
}

void debug(const FunctionCallbackInfo<Value> &args)
{
	sge_print();
}

void pack(const FunctionCallbackInfo<Value> &args) {
	Isolate *isolate = args.GetIsolate();

	if (args.Length() < 1) {
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument error.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	if (!args[0]->IsUint8Array()) {
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument 1 must be ArrayBuffer.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	int len = 0;
	Local<Uint8Array> codeArr = args[0].As<Uint8Array>();
	Local<ArrayBuffer> arr = codeArr->Buffer();
	const char* code = (const char*)arr->GetContents().Data();
	int codeLen = codeArr->Length();
	Local<ArrayBuffer> buffer = ArrayBuffer::New(isolate, BUFFER_SIZE);
	char *pBuffer = (char *)buffer->GetContents().Data();
	len = sge_pack(code, codeLen, pBuffer);

	Local<Uint8Array> u8Arr = Uint8Array::New(buffer, 0, len);
	args.GetReturnValue().Set(u8Arr);
}

void unpack(const FunctionCallbackInfo<Value> &args) {
	Isolate *isolate = args.GetIsolate();

	if (args.Length() < 1) {
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument error.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	if (!args[0]->IsUint8Array()) {
		isolate->ThrowException(Exception::TypeError(
			String::NewFromUtf8(isolate,
								"argument 1 must be ArrayBuffer.",
								NewStringType::kNormal)
				.ToLocalChecked()));
		return;
	}

	int len = 0;
	Local<Uint8Array> codeArr = args[0].As<Uint8Array>();
	Local<ArrayBuffer> arr = codeArr->Buffer();
	const char *code = (const char *)arr->GetContents().Data();
	int codeLen = codeArr->Length();
	Local<ArrayBuffer> buffer = ArrayBuffer::New(isolate, BUFFER_SIZE);
	char *pBuffer = (char *)buffer->GetContents().Data();
	len = sge_unpack(code, codeLen, pBuffer);
	Local<Uint8Array> u8Arr = Uint8Array::New(buffer, 0, len);

	args.GetReturnValue().Set(u8Arr);
}

void Initialize(Local<Object> exports)
{
	g_memPtr = malloc(sizeof(char) * BUFFER_SIZE);
	NODE_SET_METHOD(exports, "parse", parse);
	NODE_SET_METHOD(exports, "parseFile", parseFile);
	NODE_SET_METHOD(exports, "encode", encode);
	NODE_SET_METHOD(exports, "decode", decode);
	NODE_SET_METHOD(exports, "destroy", destroy);
	NODE_SET_METHOD(exports, "debug", debug);
	NODE_SET_METHOD(exports, "pack", pack);
	NODE_SET_METHOD(exports, "unpack", unpack);
}

NODE_MODULE(NODE_GYP_MODULE_NAME, Initialize)

} // namespace sgeProto
