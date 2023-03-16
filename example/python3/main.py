import time
import sgeProto

data = {
	'name': 'username1',
	'id': [13245, 1,2,34,5],
	'email': '1111112',
	'phone': [
		{ 'num': '1234', 'type': -1 },
		{ 'num': '4321', 'type': -2 }
	]
}

sgeProto.parseFile("../example.proto")
sgeProto.debug()


code = sgeProto.encode('Person', data)
pack_code = sgeProto.pack(code)
unpack_code = sgeProto.unpack(pack_code)
result = sgeProto.decode(unpack_code)

for i in range(10000000):
	code = sgeProto.encode('Person', data)
	pack_code = sgeProto.pack(code)
	unpack_code = sgeProto.unpack(pack_code)
	result = sgeProto.decode(unpack_code)
	print(result)
	time.sleep(0.2)
