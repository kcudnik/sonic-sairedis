#include "Python.h"

static PyObject *SaiRedisError;

static PyObject * sairedis_system(PyObject *self, PyObject *args);

static PyMethodDef SaiRedisMethods[] = {
    {"system",  sairedis_system, METH_VARARGS, "Execute a shell command."},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

extern "C" PyMODINIT_FUNC initsairedis(void);

PyMODINIT_FUNC initsairedis(void)
{
   PyObject *m;

   m = Py_InitModule("sairedis", SaiRedisMethods);
   if (m == NULL)
       return;

   SaiRedisError = PyErr_NewException(const_cast<char*>("sairedis.error"), NULL, NULL);
   Py_INCREF(SaiRedisError);
   PyModule_AddObject(m, "error", SaiRedisError);
}

static PyObject * sairedis_system(PyObject *self, PyObject *args)
{
    const char *command;
    int sts;

    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;

    printf("command: %s\n", command);

    sts = system(command);

    return Py_BuildValue("i", sts);
}
