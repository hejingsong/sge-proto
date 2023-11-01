import json
import sgeProto

obj1 = {
    "id": "1234",
    "type": [1,2,3,4]
}

obj2 = {
    "id": "5678",
    "type": [5,6,7,8, 9]
}

person = {
    "phone": [obj1, obj2],
    "name": "person",
    "pid": 12345,
    "email": ["aa@aa.com", "bb@bb.com"],
    "number": obj1
}

example = {
    "id": ["aa@aa.com", "bb@bb.com"],
    "name": "example"
}

p = sgeProto.parse_file("../../example/example.proto")
s = p.encode("Person", person)
o = p.decode(s.info())
print(json.dumps(o))

# s = p.encode("Example", example)
# o = p.decode(s.info())
# print(json.dumps(o))
