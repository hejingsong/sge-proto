const SgeProto = require('./build/Release/SgeProto.node');
try {
  const obj1 = {
    "id": "1234",
    "type": [1, 2, 3, 4]
  }

  const obj2 = {
    "id": "5678",
    "type": [5, 6, 7, 8, 9]
  }

  const person = {
    "phone": [obj1, obj2],
    "name": "person",
    "pid": 12345,
    "email": ["aa@aa.com", "bb@bb.com"],
    "number": obj1
  }
  const r1 = SgeProto.parse_file("/root/sge-proto/example/example.proto")
  const result = r1.encode("Person", person)
  const obj = r1.decode(result)
  console.dir(person, { depth: null })
  console.dir(obj, { depth: null })
} catch (exceptionVar) {
  console.log(exceptionVar);
}
