#include "Python.h"

extern "C" {
#include "../SAI/inc/sai.h"
}

#define _In_
#define _Out_
#define _Inout_
#define SWSS_LOG_ENTER()

//#include "swss/logger.h"

#include <map>
#include <string>


static PyObject *SaiRedisError;

static PyObject * create_switch(PyObject *self, PyObject *args);

static PyMethodDef SaiRedisMethods[] = {
    {"create_switch",  create_switch, METH_VARARGS, "Create switch."},
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

static PyObject * generic_create(
        _In_ sai_object_type_t objectType,
        _In_ PyObject *self, 
        _In_ PyObject *args)
{
    SWSS_LOG_ENTER();

    // TODO check if object type is oid

    const char *command;

    if (!PyTuple_Check(args))
    {
        PyErr_Format(SaiRedisError, "Python error, expected args type is tuple");
        return NULL;
    }

    int size = (int)PyTuple_Size(args);

    if (size != 1)
    {
        PyErr_Format(SaiRedisError, "Expected number of arguments is 1, but %d given", size);
        return NULL;
    }

    auto*dict = PyTuple_GetItem(args, 0);

    if (!PyDict_CheckExact(dict))
    {
        PyErr_Format(SaiRedisError, "Passed argument must be of type dict");
        return NULL;
    }

    std::map<std::string, std::string> map;

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(dict, &pos, &key, &value))
    {
        if (!PyString_Check(key) || !PyString_Check(value))
        {
            PyErr_Format(SaiRedisError, "Keys and values in dict must be strings");
            return NULL;
        }

        map[PyString_AsString(key)] = PyString_AsString(value);
    }

    // we got map
    // TODO check if all attributes belong to given object type
    // TODO deserialize value



    PyErr_SetString(SaiRedisError, "create command failed");
    return NULL;


    if (!PyArg_ParseTuple(args, "s", &command))
        return NULL;

    printf("command: %s\n", command);

   // sts = system(command);

    PyObject *pdict;
    pdict = PyDict_New();
    PyDict_SetItemString(pdict, "status", PyString_FromString("SAI_STATUS_SUCCESS"));
    PyDict_SetItemString(pdict, "oid", PyString_FromString("oid:0x21000000000000")); // oid: ...

    //auto* val = Py_BuildValue("i", sts);
    return pdict;
}

static PyObject* create_switch(PyObject *self, PyObject *args)
{
    SWSS_LOG_ENTER();

    return generic_create(SAI_OBJECT_TYPE_SWITCH, self, args);
}
