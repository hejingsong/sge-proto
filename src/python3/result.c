#include <Python.h>

#include "../core/proto.h"

typedef struct {
  PyObject_HEAD;
  struct sge_string *result;
} PySgeProtoResult;

static void dealloc(PySgeProtoResult *o) {
  sge_free_string(o->result);
  PyObject_Del(o);
}

static PyObject *info(PyObject *self, PyObject *args) {
  PyObject *ret = NULL;
  PySgeProtoResult *r = NULL;

  Py_INCREF(self);
  Py_XINCREF(args);

  r = (PySgeProtoResult *)self;
  ret = PyBytes_FromStringAndSize(r->result->s, r->result->l);

  Py_DECREF(self);
  Py_XDECREF(args);

  return ret;
}

PyDoc_STRVAR(doc, "");

static PyMethodDef methods[] = {{"info", info, METH_NOARGS, "proto result"},
                                {NULL, NULL}};

static PyTypeObject PyProtoResult_Type = {
    PyVarObject_HEAD_INIT(NULL, 0) "sgeProto.ProtoResult",
    sizeof(PySgeProtoResult),
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
    doc,                                      /* tp_doc */
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

int init_result_type(PyObject *module) {
  if (PyModule_AddType(module, &PyProtoResult_Type)) {
    return -1;
  }

  return 0;
}

PyObject *new_proto_result(struct sge_string *s) {
  PyObject *obj = NULL;
  PySgeProtoResult *r = NULL;

  obj = PyObject_MALLOC(sizeof(PyProtoResult_Type));
  if (!obj) {
    PyErr_NoMemory();
    return NULL;
  }

  r = (PySgeProtoResult *)PyObject_INIT(obj, &PyProtoResult_Type);
  r->result = s;

  return obj;
}
