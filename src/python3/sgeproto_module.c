#include <Python.h>

#include "../core/sge_proto.h"

#define BUFFER_SIZE	2048

static void
py_field_get(const void *pyObject, sge_value* ud) {
	const char *field_name = ud->name;
	PyObject *key = NULL;
	PyObject *value = NULL;
	PyObject *object = (PyObject *)pyObject;

	key = PyUnicode_FromString(field_name);

	if (PyDict_Check(object)) {
		value = PyDict_GetItem(object, key);
	} else if (PyList_Check(object)) {
		value = PyList_GetItem(object, ud->idx);
	}

	if (NULL == value) {
		goto RET;
	}
	if (PyLong_Check(value)) {
		*((long *)ud->ptr) = PyLong_AS_LONG(value);
	} else if (PyUnicode_Check(value)) {
		ud->ptr = PyUnicode_AsUTF8AndSize(value, (Py_ssize_t *)&ud->len);
	} else if (PyBytes_Check(value)) {
		ud->ptr = (void *)PyBytes_AsString(value);
		ud->len = PyBytes_Size(value);
	} else if (PyList_Check(value)) {
		ud->ptr = (void *)value;
		ud->len = Py_SIZE(value);
	} else if (PyDict_Check(value)) {
		ud->ptr = (void *)value;
		ud->len = PyDict_Size(value);
	}
RET:
	Py_XDECREF(key);
	return;
}

static void *
py_field_set(void *pyObject, sge_value *ud) {
	PyObject *key = NULL;
	PyObject *object = (PyObject *)pyObject;
	PyObject *value = NULL;

	switch (ud->vt) {
		case SGE_NUMBER:
			value = PyLong_FromLong(*((long *)ud->ptr));
			break;
		case SGE_STRING:
			value = PyUnicode_FromStringAndSize(ud->ptr, ud->len);
			break;
		case SGE_DICT:
			value = PyDict_New();
			break;
		case SGE_LIST:
			value = PyList_New(ud->len);
			break;
	}

	if (ud->idx == -1) {
		key = PyUnicode_FromString(ud->name);
		PyDict_SetItem(object, key, value);
	}
	else {
		Py_INCREF(value);
		PyList_SetItem(object, ud->idx, value);
	}

	Py_XDECREF(value);
	Py_XDECREF(key);
	return value;
}

PyObject *
py_sge_parse(PyObject *self, PyObject *buffer) {
	const char *buf = NULL;
	int ret = 0;

	if (!PyUnicode_Check(buffer)) {
		PyErr_Format(PyExc_TypeError, "only accept str object");
		Py_RETURN_FALSE;
	}

	buf = PyUnicode_AsUTF8(buffer);
	ret = sge_parse((char*)buf);
	if (ret == SGE_OK) {
		Py_RETURN_TRUE;
	} else {
		const char* err = sge_error(ret);
		PyErr_Format(PyExc_RuntimeError, err);
		Py_RETURN_FALSE;
	}
}

PyObject *
py_sge_parse_file(PyObject *self, PyObject *file) {
	int ret = 0;
	const char *filename = NULL;

	if (!PyUnicode_Check(file)) {
		PyErr_Format(PyExc_TypeError, "only accept str object");
		Py_RETURN_FALSE;
	}
	filename = PyUnicode_AsUTF8(file);
	ret = sge_parse_file(filename);
	if (ret == SGE_OK) {
		Py_RETURN_TRUE;
	}

	const char* err = sge_error(ret);
	PyErr_Format(PyExc_RuntimeError, err);
	Py_RETURN_FALSE;
}

PyObject *
py_sge_encode(PyObject *self, PyObject *args) {
	int size = 0;
	char buffer[BUFFER_SIZE];
	const char *name;
	PyObject *proto_name;
	PyObject *userdata;
	PyObject *buf_obj = NULL;

	if (!PyArg_ParseTuple(args, "UO", &proto_name, &userdata)) {
		PyErr_Format(PyExc_TypeError, "args 1 must be str. args 2 must be dict");
		Py_RETURN_FALSE;
	}

	if (!PyUnicode_Check(proto_name)) {
		PyErr_Format(PyExc_TypeError, "args 1 must be str");
		Py_RETURN_FALSE;
	}
	if (!PyDict_Check(userdata)) {
		PyErr_Format(PyExc_TypeError, "args 2 must be dict");
		Py_RETURN_FALSE;
	}

	memset(buffer, 0, BUFFER_SIZE);
	name = PyUnicode_AsUTF8(proto_name);
	size = sge_encode(name, userdata, buffer, py_field_get);
	if (size <= 0) {
		const char* err = sge_error(size);
		PyErr_Format(PyExc_RuntimeError, err);
		goto ERR;
	}
	buf_obj = PyBytes_FromStringAndSize(buffer, size);
	ERR:
	return buf_obj;
}

PyObject *
py_sge_decode(PyObject *self, PyObject *buf_obj) {
	PyObject *object, *proto_obj, *ret;
	int proto_idx;
	char *buffer = NULL;

	if (!PyBytes_Check(buf_obj)) {
		PyErr_Format(PyExc_TypeError, "args 1 must be bytes.");
		Py_RETURN_FALSE;
	}

	buffer = PyBytes_AsString(buf_obj);
	object = PyDict_New();
	if (object == NULL) {
		Py_RETURN_FALSE;
	}

	proto_idx = sge_decode(buffer, object, py_field_set);
	if (proto_idx < 0) {
		Py_DECREF(object);
		const char* err = sge_error(proto_idx);
		PyErr_Format(PyExc_RuntimeError, err);
		Py_RETURN_FALSE;
	}

	proto_obj = PyLong_FromLong(proto_idx);
	ret = PyTuple_New(2);
	PyTuple_SetItem(ret, 0, proto_obj);
	PyTuple_SetItem(ret, 1, object);

	return ret;
}

PyObject *
py_sge_destroy(PyObject *self, PyObject *args) {
	sge_destroy(1);
	Py_RETURN_NONE;
}

PyObject *
py_sge_debug(PyObject *self, PyObject *args) {
	sge_print();
	Py_RETURN_NONE;
}

PyObject*
py_sge_pack(PyObject *self, PyObject *args) {
	PyObject* out_byte = NULL;
	char* buf = NULL;
	char outbuf[BUFFER_SIZE];
	int outlen = 0;
	Py_ssize_t buflen = 0;

	if (!PyBytes_Check(args)) {
		PyErr_Format(PyExc_TypeError, "args 1 must be bytes.");
		Py_RETURN_FALSE;
	}

	memset(outbuf, 0, BUFFER_SIZE);
	PyBytes_AsStringAndSize(args, &buf, &buflen);
	outlen = sge_pack((const char*)buf, buflen, outbuf);
	if (outlen < 0) {
		const char* err = sge_error(outlen);
		PyErr_Format(PyExc_RuntimeError, err);
		goto ERROR;
	}

	out_byte = PyBytes_FromStringAndSize((const char*)outbuf, outlen);
	ERROR:
	return out_byte;
}

PyObject*
py_sge_unpack(PyObject *self, PyObject *args) {
	PyObject* out_byte = NULL;
	char* buf = NULL;
	char outbuf[BUFFER_SIZE];
	int outlen = 0;
	Py_ssize_t buflen = 0;

	if (!PyBytes_Check(args)) {
		PyErr_Format(PyExc_TypeError, "args 1 must be bytes.");
		Py_RETURN_FALSE;
	}

	memset(outbuf, 0, BUFFER_SIZE);
	PyBytes_AsStringAndSize(args, &buf, &buflen);
	outlen = sge_unpack((const char*)buf, buflen, outbuf);
	if (outlen < 0) {
		const char* err = sge_error(outlen);
		PyErr_Format(PyExc_RuntimeError, err);
		goto ERROR;
	}

	out_byte = PyBytes_FromStringAndSize((const char*)outbuf, outlen);
	ERROR:
	return out_byte;
}

static PyMethodDef sgeProtoMethods[] = {
	{"parse", py_sge_parse, METH_O, "sg protocol parse from string buffer"},
	{"parseFile", py_sge_parse_file, METH_O, "sg protocol parse from file"},
	{"encode", py_sge_encode, METH_VARARGS, "sg protocol encode"},
	{"decode", py_sge_decode, METH_O, "sg protocol decode"},
	{"destory", py_sge_destroy, METH_NOARGS, "destory sg protocol table"},
	{"debug", py_sge_debug, METH_NOARGS, "debug"},
	{"pack", py_sge_pack, METH_O, "pack"},
	{"unpack", py_sge_unpack, METH_O, "unpack"},
	{NULL, NULL, 0, NULL}
};

static struct PyModuleDef sgeProtoModule = {
	PyModuleDef_HEAD_INIT,
	"sgeProto",
	"Python interface for sgeProto",
	-1,
	sgeProtoMethods
};

PyMODINIT_FUNC PyInit_sgeProto(void) {
	return PyModule_Create(&sgeProtoModule);
}
