const sgeProto = require('../../src/node/build/Release/sgeProto.node');

const data = {
	'name': 'username',
	'id': [13245, 1,2,34,5],
	'email': '111111',
	'phone': [
		{ 'num': '123', 'type': 1 },
		{ 'num': '456', 'type': 2 },
	]
}

const data1 = {
	'name': 'username1',
	'id': [13245, 1,2,34,5],
	'email': '1111112',
	'phone': [
		{ 'num': '1234', 'type': -1 },
		{ 'num': '4321', 'type': -2 }
	]
}

sgeProto.parseFile("../example.proto");

const code = sgeProto.encode("Person", data);
const pack_code = sgeProto.pack(code)
const unpack_code = sgeProto.unpack(pack_code)
const result = sgeProto.decode(unpack_code)
console.log(result[0])
console.log(result[1])

const code1 = sgeProto.encode("Person", data1);
const pack_code1 = sgeProto.pack(code1)
const unpack_code1 = sgeProto.unpack(pack_code1)
const result1 = sgeProto.decode(unpack_code1)
console.log(result1[0])
console.log(result1[1])

sgeProto.destroy();
