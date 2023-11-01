#include <Python.h>

#include "../core/proto.h"

typedef struct {
  PyObject_HEAD;
  struct sge_proto *proto;
} PySgeProto;

int init_result_type(PyObject *module);
PyObject *new_proto_result(struct sge_string *s);

static int get(const void *ud, const struct sge_key *k, struct sge_value *v) {
  PyObject *obj = (PyObject *)ud;
  PyObject *val = NULL;
  sge_integer n = 0;
  const char *s = NULL;
  Py_ssize_t len = 0;

  val = PyDict_GetItemString(obj, k->name.s);
  if (k->t == KT_LIST_INDEX) {
    val = PyList_GET_ITEM(val, k->idx);
  }

  if (NULL == val) {
    sge_value_nil(v);
    return SGE_OK;
  }

  if (PyLong_Check(val)) {
    sge_value_integer(v, PyLong_AS_LONG(val));
  } else if (PyUnicode_Check(val)) {
    s = PyUnicode_AsUTF8AndSize(val, &len);
    sge_value_string_with_len(v, s, len);
  } else if (PyBytes_Check(val)) {
    PyBytes_AsStringAndSize(val, (char **)&s, &len);
    sge_value_string_with_len(v, s, len);
  } else if (PyList_Check(val)) {
    n = PyList_GET_SIZE(val);
    sge_value_integer(v, n);
  } else if (PyDict_Check(val)) {
    sge_value_any(v, val);
  } else {
    sge_value_nil(v);
    return SGE_ERR;
  }

  return SGE_OK;
}

static void *set(void *ud, const struct sge_key *k, const struct sge_value *v) {
  int t = sge_value_type(v->t);
  int is_arr = sge_is_list(v->t);
  void *ret = ud;
  PyObject *val = NULL;
  PyObject *obj = (PyObject *)ud;

  if (is_arr) {
    val = PyList_New(v->v.i);
    ret = val;
  } else {
    switch (t) {
      case FIELD_TYPE_INTEGER:
        val = PyLong_FromLongLong(v->v.i);
        break;

      case FIELD_TYPE_STRING:
        val = PyUnicode_FromStringAndSize(v->v.s.s, v->v.s.l);
        break;

      case FIELD_TYPE_UNKNOWN:
        val = Py_None;
        Py_INCREF(Py_None);
        break;

      case FIELD_TYPE_CUSTOM:
        val = PyDict_New();
        ret = val;
        break;
    }
  }

  if (NULL == val) {
    return NULL;
  }

  if (k->t == KT_STRING) {
    PyDict_SetItemString(obj, k->name.s, val);
  } else if (k->t == KT_LIST_INDEX) {
    Py_INCREF(val);
    PyList_SET_ITEM(obj, k->idx, val);
  }

  Py_DECREF(val);
  return ret;
}

PyDoc_STRVAR(
    sge_proto_doc,
    "dict() -> new empty dictionary\n"
    "dict(mapping) -> new dictionary initialized from a mapping object's\n"
    "    (key, value) pairs\n"
    "dict(iterable) -> new dictionary initialized as if via:\n"
    "    d = {}\n"
    "    for k, v in iterable:\n"
    "        d[k] = v\n"
    "dict(**kwargs) -> new dictionary initialized with the name=value pairs\n"
    "    in the keyword argument list.  For example:  dict(one=1, two=2)");

static void dealloc(PySgeProto *o) {
  sge_free_proto(o->proto);
  PyObject_Del(o);
}

static PyObject *encode(PyObject *self, PyObject *args) {
  int err_code = 0;
  PyObject *ud = NULL;
  PyObject *proto_name = NULL;
  PyObject *ret = NULL;
  PySgeProto *proto = NULL;
  struct sge_string *s = NULL;
  const char *s_proto_name = NULL;
  const char *err = NULL;

  if (!PyArg_ParseTuple(args, "UO", &proto_name, &ud)) {
    PyErr_Format(PyExc_TypeError, "args 1 must be str. args 2 must be dict");
    Py_RETURN_NONE;
  }

  if (!PyUnicode_Check(proto_name)) {
    PyErr_Format(PyExc_TypeError, "args 1 must be str");
    Py_RETURN_NONE;
  }
  if (!PyDict_Check(ud)) {
    PyErr_Format(PyExc_TypeError, "args 2 must be dict");
    Py_RETURN_NONE;
  }

  Py_INCREF(self);
  Py_INCREF(ud);
  Py_INCREF(proto_name);

  proto = (PySgeProto *)self;
  s_proto_name = PyUnicode_AsUTF8(proto_name);
  s = sge_encode(proto->proto, s_proto_name, ud, get);
  if (NULL == s) {
    err_code = sge_proto_error(proto->proto, &err);
    PyErr_Format(PyExc_RuntimeError, "encode error(%d), msg(%s).", err_code,
                 err);
    goto out;
  }

  ret = new_proto_result(s);
  if (NULL == ret) {
    PyErr_NoMemory();
  }

out:
  Py_DECREF(self);
  Py_DECREF(ud);
  Py_DECREF(proto_name);

  return ret;
}

static PyObject *decode(PyObject *self, PyObject *buffer) {
  int err_code = 0;
  char *s = NULL;
  const char *err = NULL;
  Py_ssize_t len = 0;
  PyObject *obj = NULL;
  PySgeProto *proto = NULL;

  if (!PyBytes_Check(buffer)) {
    PyErr_Format(PyExc_TypeError, "args 1 must be bytes.");
    Py_RETURN_NONE;
  }

  Py_INCREF(self);
  Py_INCREF(buffer);

  PyBytes_AsStringAndSize(buffer, &s, &len);
  obj = PyDict_New();
  if (obj == NULL) {
    PyErr_NoMemory();
    goto out;
  }

  proto = (PySgeProto *)self;
  if (SGE_OK != sge_decode(proto->proto, (const char *)s, len, obj, set)) {
    err_code = sge_proto_error(proto->proto, &err);
    PyErr_Format(PyExc_RuntimeError, "decode error(%d). msg(%s).", err_code,
                 err);
    Py_DECREF(obj);
    obj = NULL;
  }

out:
  Py_DECREF(self);
  Py_DECREF(buffer);

  return obj;
}

static PyMethodDef methods[] = {
    {"encode", encode, METH_VARARGS, "sg protocol encode"},
    {"decode", decode, METH_O, "sg protocol decode"},
    {NULL, NULL}};

PyTypeObject PyProto_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "sgeProto.Proto",
    sizeof(PySgeProto),
    0,
    (destructor)dealloc,                      /* tp_dealloc */
    0,                                        /* tp_vectorcall_offset */
    0,                                        /* tp_getattr */
    0,                                        /* tp_setattr */
    0,                                        /* tp_as_async */
    0,                                        /* tp_repr */
    0,                                        /* tp_as_number */
    0,                                        /* tp_as_sequence */
    0,                                        /* tp_as_mapping */
    0,                                        /* tp_hash */
    0,                                        /* tp_call */
    0,                                        /* tp_str */
    0,                                        /* tp_getattro */
    0,                                        /* tp_setattro */
    0,                                        /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* tp_flags */
    sge_proto_doc,                            /* tp_doc */
    0,                                        /* tp_traverse */
    0,                                        /* tp_clear */
    0,                                        /* tp_richcompare */
    0,                                        /* tp_weaklistoffset */
    0,                                        /* tp_iter */
    0,                                        /* tp_iternext */
    methods,                                  /* tp_methods */
    0,                                        /* tp_members */
    0,                                        /* tp_getset */
    0,                                        /* tp_base */
    0,                                        /* tp_dict */
    0,                                        /* tp_descr_get */
    0,                                        /* tp_descr_set */
    0,                                        /* tp_dictoffset */
    0,                                        /* tp_init */
    0,                                        /* tp_alloc */
    0,                                        /* tp_new */
    PyObject_Del,                             /* tp_free */
};

static PyObject *parse(PyObject *self, PyObject *buffer) {
  int ret = 0;
  Py_ssize_t len = 0;
  const char *bufp = NULL, *err = NULL;
  struct sge_proto *proto = NULL;
  PyObject *obj = NULL;
  PySgeProto *proto_obj = NULL;

  if (!PyUnicode_Check(buffer)) {
    PyErr_Format(PyExc_TypeError, "only accept str object");
    Py_RETURN_NONE;
  }

  Py_INCREF(self);
  Py_INCREF(buffer);

  bufp = PyUnicode_AsUTF8AndSize(buffer, &len);
  proto = sge_parse_content(bufp, len);
  if (NULL == proto) {
    PyErr_NoMemory();
    goto out;
  }
  ret = sge_proto_error(proto, &err);
  if (SGE_OK != ret) {
    PyErr_Format(PyExc_RuntimeError, err);
    sge_free_proto(proto);
    goto out;
  }

  obj = PyObject_MALLOC(sizeof(PySgeProto));
  if (!obj) {
    PyErr_NoMemory();
    sge_free_proto(proto);
    goto out;
  }

  proto_obj = (PySgeProto *)PyObject_INIT(obj, &PyProto_Type);
  proto_obj->proto = proto;

out:
  Py_DECREF(self);
  Py_DECREF(buffer);

  return obj;
}

static PyObject *parse_file(PyObject *self, PyObject *file) {
  int ret = 0;
  const char *filename = NULL, *err = NULL;
  struct sge_proto *proto = NULL;
  PyObject *obj = NULL;
  PySgeProto *proto_obj = NULL;

  if (!PyUnicode_Check(file)) {
    PyErr_Format(PyExc_TypeError, "only accept str object");
    Py_RETURN_FALSE;
  }

  Py_INCREF(self);
  Py_INCREF(file);

  filename = PyUnicode_AsUTF8(file);
  proto = sge_parse(filename);
  if (NULL == proto) {
    PyErr_NoMemory();
    goto out;
  }

  ret = sge_proto_error(proto, &err);
  if (0 != ret) {
    PyErr_Format(PyExc_RuntimeError, err);
    sge_free_proto(proto);
    goto out;
  }

  obj = PyObject_MALLOC(sizeof(PySgeProto));
  if (!obj) {
    PyErr_NoMemory();
    sge_free_proto(proto);
    goto out;
  }

  proto_obj = (PySgeProto *)PyObject_INIT(obj, &PyProto_Type);
  proto_obj->proto = proto;

out:
  Py_DECREF(self);
  Py_DECREF(file);

  return obj;
}

static PyObject *debug(PyObject *self, PyObject *args) {
  PySgeProto *proto = NULL;

  Py_INCREF(self);
  Py_INCREF(args);

  proto = (PySgeProto *)self;
  sge_print_proto(proto->proto);

  Py_DECREF(self);
  Py_DECREF(args);

  Py_RETURN_NONE;
}

static PyMethodDef sge_proto_methods[] = {
    {"parse", parse, METH_O, "sg protocol parse from string buffer"},
    {"parse_file", parse_file, METH_O, "sg protocol parse from file"},
    {"debug", debug, METH_NOARGS, "debug"},
    {NULL, NULL, 0, NULL}};

static struct PyModuleDef sge_proto_module = {PyModuleDef_HEAD_INIT, "sgeProto",
                                              "Python interface for sgeProto",
                                              -1, sge_proto_methods};

PyMODINIT_FUNC PyInit_sgeProto(void) {
  PyObject *module = NULL;

  module = PyModule_Create(&sge_proto_module);
  if (NULL == module) {
    return NULL;
  }

  if (PyModule_AddType(module, &PyProto_Type)) {
    return NULL;
  }

  if (0 != init_result_type(module)) {
    return NULL;
  }

  return module;
}
