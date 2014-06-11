/**
 * Python interface to BFI library
 */

#include <Python.h>
#include <string.h>
#include <stdio.h>
#include "bfi.h"

static char module_docstring[] =
    "Interface module for Bloom Filter Index functions";
static char bfi_open_docstring[] =
    "Open a BFI file.";
static char bfi_close_docstring[] =
    "Flush and close a BFI file.";
static char bfi_sync_docstring[] =
    "Flush changes to disk.";
static char bfi_append_docstring[] =
    "Adds entry to the end of index without checking pk.";
static char bfi_insert_docstring[] =
    "Updates or appends entry by pk";
static char bfi_delete_docstring[] =
    "Remove an entry from the index by pk";
static char bfi_lookup_docstring[] =
    "Retrieve all the possible primary keys for a set of values.";
static char bfi_stat_docstring[] =
    "Get stats for a BFI index";

static char bfi_cap_ptr[] = "BFI.ptr";

static PyObject *bfi_bfi_open(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_close(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_sync(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_append(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_insert(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_delete(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_lookup(PyObject *self, PyObject *args);
static PyObject *bfi_bfi_stat(PyObject *self, PyObject *args);

static PyMethodDef module_methods[] = {
    {"bfi_open", bfi_bfi_open, METH_VARARGS, bfi_open_docstring},
    {"bfi_close", bfi_bfi_close, METH_VARARGS, bfi_close_docstring},
    {"bfi_sync", bfi_bfi_sync, METH_VARARGS, bfi_sync_docstring},
    {"bfi_append", bfi_bfi_append, METH_VARARGS, bfi_append_docstring},
    {"bfi_insert", bfi_bfi_insert, METH_VARARGS, bfi_insert_docstring},
    {"bfi_delete", bfi_bfi_delete, METH_VARARGS, bfi_delete_docstring},
    {"bfi_lookup", bfi_bfi_lookup, METH_VARARGS, bfi_lookup_docstring},
    {"bfi_stat", bfi_bfi_stat, METH_VARARGS, bfi_stat_docstring},
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

    bfi * index = bfi_open(filename, BFI_FORMAT_128);

    if(index == NULL) {
        char * message;
        switch(errno) {
            case BFI_ERR_MAGIC:
                message = "Not a bloom index";
                break;
            case BFI_ERR_VERSION:
                message = "Incompatible file version";
                break;
            case BFI_ERR_FORMAT:
                message = "Incompatible bloom format";
                break;
            default:
                message = strerror(errno);
        }
        PyErr_SetString(PyExc_IOError, message);
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

static PyObject *bfi_bfi_append(PyObject *self, PyObject *args) {
    PyObject *cap;
    bfi * index;
    int pk, c, i;
    PyObject *values;
    char ** buf;

    if(!PyArg_ParseTuple(args, "OiO", &cap, &pk, &values)) return NULL;
    if((index = PyCapsule_GetPointer(cap, bfi_cap_ptr)) == NULL) return NULL;

    c = PyList_Size(values);
    if(c<1) {
        PyErr_SetString(PyExc_ValueError, "Need at least one value to index.");
        return NULL;
    }
    buf = malloc(sizeof(char*) * c);
    for(i=0; i<c; i++) {
        buf[i] = PyString_AsString(PyList_GetItem(values, i));
    }
    //dump_strings(buf, c);

    int result = bfi_append(index, pk, buf, c);
    free(buf);

    PyObject *ret = Py_BuildValue("i", result);
    return ret;
}

static PyObject *bfi_bfi_insert(PyObject *self, PyObject *args) {
    PyObject *cap;
    bfi * index;
    int pk, c, i;
    PyObject *values;
    char ** buf;

    if(!PyArg_ParseTuple(args, "OiO", &cap, &pk, &values)) return NULL;
    if((index = PyCapsule_GetPointer(cap, bfi_cap_ptr)) == NULL) return NULL;

    c = PyList_Size(values);
    if(c<1) {
        PyErr_SetString(PyExc_ValueError, "Need at least one value to index.");
        return NULL;
    }
    buf = malloc(sizeof(char*) * c);
    for(i=0; i<c; i++) {
        buf[i] = PyString_AsString(PyList_GetItem(values, i));
    }
    //dump_strings(buf, c);

    int result = bfi_insert(index, pk, buf, c);
    free(buf);

    PyObject *ret = Py_BuildValue("i", result);
    return ret;
}

static PyObject *bfi_bfi_delete(PyObject *self, PyObject *args) {
    PyObject *cap;
    bfi * index;
    int pk;

    if(!PyArg_ParseTuple(args, "Oi", &cap, &pk)) return NULL;
    if((index = PyCapsule_GetPointer(cap, bfi_cap_ptr)) == NULL) return NULL;

    PyObject *ret = Py_BuildValue("i", bfi_delete(index, pk));
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
    if(c<1) {
        PyErr_SetString(PyExc_ValueError, "Need at least one value to lookup.");
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

static PyObject *bfi_bfi_stat(PyObject *self, PyObject *args) {
    PyObject *cap;
    bfi * index;

    if(!PyArg_ParseTuple(args, "O", &cap)) return NULL;
    if((index = PyCapsule_GetPointer(cap, bfi_cap_ptr)) == NULL) return NULL;

    PyObject *ret = Py_BuildValue("{s:i,s:i,s:i,s:i,s:i,s:i,s:i}",
            "version", index->version,
            "records", index->records - index->deleted,
            "pages", index->total_pages,
            "records_per_page", BFI_RECORDS_PER_PAGE,
            "bloom_size", BLOOM_SIZE,
            "page_size", BFI_PAGE_SIZE,
            "size", BFI_HEADER + (BFI_PAGE_SIZE * index->total_pages) + 1);
    return ret;
}
