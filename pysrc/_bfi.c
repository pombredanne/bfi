/**
 * Python interface to BFI library
 */
 
#include <Python.h>
#include <string.h>
#include <stdio.h>
#include "../src/bfi.h"

static char module_docstring[] =
    "Interface module for Bloom Filter Index functions";
static char bfi_open_docstring[] =
    "Open a BFI file.";
static char bfi_close_docstring[] =
    "Flush and close a BFI file.";
static char bfi_sync_docstring[] =
    "Flush changes to disk.";
static char bfi_index_docstring[] =
    "Insert or update an index by primary key.";
static char bfi_lookup_docstring[] =
    "Retrieve all the possible primary keys for a set of values.";

static char bfi_cap_ptr[] = "BFI.ptr";

static PyObject *bfi_bfi_open(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_close(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_sync(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_index(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_lookup(PyObject *self, PyObject *args);

static PyMethodDef module_methods[] = {
    {"bfi_open", bfi_bfi_open, METH_VARARGS, bfi_open_docstring},
    {"bfi_close", bfi_bfi_close, METH_VARARGS, bfi_close_docstring},
    {"bfi_sync", bfi_bfi_sync, METH_VARARGS, bfi_sync_docstring},
    {"bfi_index", bfi_bfi_index, METH_VARARGS, bfi_index_docstring},
    {"bfi_lookup", bfi_bfi_lookup, METH_VARARGS, bfi_lookup_docstring},
    {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC init_bfi(void) {
    PyObject *m = Py_InitModule3("_bfi", module_methods, module_docstring);
    if (m == NULL)
        return;
}

/* Actual bindings */

static PyObject *bfi_bfi_open(PyObject *self, PyObject *args) {
    char * filename;
    
    if(!PyArg_ParseTuple(args, "s", &filename)) return NULL;
    
    bfi * index = bfi_open(filename);
    
    if(index == NULL) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to open index.");
        return NULL;
    }
    
    PyObject *cap = PyCapsule_New(index, bfi_cap_ptr, NULL);
    PyObject *ret = Py_BuildValue("O", cap);
    return ret;
}

static PyObject *bfi_bfi_close(PyObject *self, PyObject *args) {
    PyObject *cap;
    bfi * index;
    
    if(!PyArg_ParseTuple(args, "O", &cap)) return NULL;
    if((index = PyCapsule_GetPointer(cap, bfi_cap_ptr)) == NULL) return NULL;
    
    bfi_close(index);
    
    PyObject *ret = Py_BuildValue("i", 0);
    return ret;
}

static PyObject *bfi_bfi_sync(PyObject *self, PyObject *args) {
    PyObject *cap;
    bfi * index;
    
    if(!PyArg_ParseTuple(args, "O", &cap)) return NULL;
    if((index = PyCapsule_GetPointer(cap, bfi_cap_ptr)) == NULL) return NULL;
    
    PyObject *ret = Py_BuildValue("i", bfi_sync(index));
    return ret;
}

void dump_strings(char ** input, int len) {
    int i;
    for(i=0; i<len; i++) printf("%s\n", input[i]);
}

static PyObject *bfi_bfi_index(PyObject *self, PyObject *args) {
    PyObject *cap;
    bfi * index;
    int pk, c, i;
    PyObject *values;
    char ** buf;
    
    if(!PyArg_ParseTuple(args, "OiO", &cap, &pk, &values)) return NULL;
    if((index = PyCapsule_GetPointer(cap, bfi_cap_ptr)) == NULL) return NULL;
    
    c = PyList_Size(values);
    if(!c) {
        PyErr_SetString(PyExc_RuntimeError, "Need at least one value to index.");
        return NULL;
    }
    buf = malloc(sizeof(char*) * c);
    for(i=0; i<c; i++) {
        buf[i] = PyString_AsString(PyList_GetItem(values, i));
    }
    //dump_strings(buf, c);
    
    int result = bfi_index(index, pk, buf, c);
    
    PyObject *ret = Py_BuildValue("i", result);
    return ret;
}

static PyObject *bfi_bfi_lookup(PyObject *self, PyObject *args) {
    PyObject *cap;
    bfi * index;
    PyObject *values;
    char ** buf;
    uint32_t * result;
    int c, i;
    
    if(!PyArg_ParseTuple(args, "OO", &cap, &values)) return NULL;
    if((index = PyCapsule_GetPointer(cap, bfi_cap_ptr)) == NULL) return NULL;
    
    c = PyList_Size(values);
    if(!c) {
        PyErr_SetString(PyExc_RuntimeError, "Need at least one value to lookup.");
        return NULL;
    }
    buf = malloc(sizeof(char*) * c);
    for(i=0; i<c; i++) {
        buf[i] = PyString_AsString(PyList_GetItem(values, i));
    }
    //dump_strings(buf, c);
    
    c = bfi_lookup(index, buf, c, &result);
    
    // build the result
    PyObject *list = PyList_New(c);
    for(i=0; i<c; i++) PyList_SetItem(list, i, PyInt_FromLong(result[i]));
    
    PyObject *ret = Py_BuildValue("O", list);
    return ret;
}
