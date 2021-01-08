### Introduction
A simple serialization tool.

### Schema
```
# i am comment
PhoneNumber 1 {
    num: string;
    type: number8;      # 8 bit number like int8_t; number16 => int16_t, number32 => int32_t, number => int32_t
}

Person 2 {
    id : number;
    name : string;
    phone: PhoneNumber[];   # PhoneNumber list
}
```

### use in python3
```
cd src/python3
python3 setup.py install
cd ../../example/python3
python3 main.py
```

### use in node-v12.14.1
```
cd src/node
node-gyp configure
node-gyp build
cd ../../example/python
node main.js
```

### TODO LIST
1. Improve the expression of error messages
