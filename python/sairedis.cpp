#include "Python.h"

extern "C" {
#include "sai.h"
#include "saimetadata.h"
}

#include "../lib/inc/Sai.h"
#include "../meta/sai_serialize.h"
#include "swss/logger.h"

#include <map>
#include <string>
#include <memory>
#include <vector>

static std::map<std::string, std::string> g_profileMap;

static const char *profile_get_value (
        _In_ sai_switch_profile_id_t profile_id,
        _In_ const char *variable)
{
    SWSS_LOG_ENTER();

    auto it = g_profileMap.find(variable);

    if (it == g_profileMap.end())
        return NULL;
    return it->second.c_str();
}

static int profile_get_next_value (
        _In_ sai_switch_profile_id_t profile_id,
        _Out_ const char **variable,
        _Out_ const char **value)
{
    SWSS_LOG_ENTER();

    static auto it = g_profileMap.begin();

    if (value == NULL)
    {
        // Restarts enumeration
        it = g_profileMap.begin();
    }
    else if (it == g_profileMap.end())
    {
        return -1;
    }
    else
    {
        *variable = it->first.c_str();
        *value = it->second.c_str();
        it++;
    }

    if (it != g_profileMap.end())
        return 0;
    else
        return -1;
}

static const sai_service_method_table_t service_method_table = {
    profile_get_value,
    profile_get_next_value
};

static std::shared_ptr<sairedis::Sai> g_sai;

static PyObject* SaiRedisError;

// create
static PyObject* create_switch(PyObject *self, PyObject *args);
static PyObject* create_vlan(PyObject *self, PyObject *args);

// remove
static PyObject* remove_switch(PyObject *self, PyObject *args);
static PyObject* remove_vlan(PyObject *self, PyObject *args);

static PyMethodDef SaiRedisMethods[] = {

    {"create_switch",  create_switch, METH_VARARGS, "Create switch."},
    {"create_vlan",    create_vlan,   METH_VARARGS, "Create vlan."},

    {"remove_switch",  remove_switch, METH_VARARGS, "Remove switch."},
    {"remove_vlan",    remove_vlan,   METH_VARARGS, "Remove vlan."},

    {NULL, NULL, 0, NULL}        // sentinel
};

extern "C" PyMODINIT_FUNC initsairedis(void);

PyMODINIT_FUNC initsairedis(void)
{
    SWSS_LOG_ENTER();

    PyObject *m;

    m = Py_InitModule("sairedis", SaiRedisMethods);
    if (m == NULL)
        return;

    g_sai = std::make_shared<sairedis::Sai>();

    g_sai->initialize(0, &service_method_table);

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

    sai_object_id_t switchId = SAI_NULL_OBJECT_ID;

    auto* info = sai_metadata_get_object_type_info(objectType);

    if (!info)
    {
        PyErr_Format(SaiRedisError, "Invalid object type specified");
        return nullptr;
    }

    if (info->isnonobjectid)
    {
        PyErr_Format(SaiRedisError, "Non object id specified to oid function");
        return nullptr;
    }

    if (!PyTuple_Check(args))
    {
        PyErr_Format(SaiRedisError, "Python error, expected args type is tuple");
        return nullptr;
    }

    int size = (int)PyTuple_Size(args);

    PyObject* dict = nullptr;

    if (objectType == SAI_OBJECT_TYPE_SWITCH)
    {
        if (size != 1)
        {
            PyErr_Format(SaiRedisError, "Expected number of arguments is 1, but %d given", size);
            return nullptr;
        }

        dict = PyTuple_GetItem(args, 0);
    }
    else
    {
        if (size != 2)
        {
            PyErr_Format(SaiRedisError, "Expected number of arguments is 2, but %d given", size);
            return nullptr;
        }

        auto*swid = PyTuple_GetItem(args, 0);

        if (!PyString_Check(swid))
        {
            PyErr_Format(SaiRedisError, "Switch id must be string type");
            return nullptr;
        }

        try
        {
            sai_deserialize_object_id(PyString_AsString(swid), &switchId);
        }
        catch (const std::exception&e)
        {
            PyErr_Format(SaiRedisError, "Failed to deserialize switchId: %s", e.what());
            return nullptr;
        }

        dict = PyTuple_GetItem(args, 1);
    }

    if (!PyDict_CheckExact(dict))
    {
        PyErr_Format(SaiRedisError, "Passed argument must be of type dict");
        return nullptr;
    }

    std::map<std::string, std::string> map;

    PyObject *key, *value;
    Py_ssize_t pos = 0;

    while (PyDict_Next(dict, &pos, &key, &value))
    {
        if (!PyString_Check(key) || !PyString_Check(value))
        {
            PyErr_Format(SaiRedisError, "Keys and values in dict must be strings");
            return nullptr;
        }

        // save pair to map
        map[PyString_AsString(key)] = PyString_AsString(value);
    }

    std::vector<sai_attribute_t> attrs;
    std::vector<const sai_attr_metadata_t*> meta;

    for (auto& kvp: map)
    {
        auto*md = sai_metadata_get_attr_metadata_by_attr_id_name(kvp.first.c_str());

        if (!md)
        {
            PyErr_Format(SaiRedisError, "Invalid attribute: %s", kvp.first.c_str());
            return nullptr;
        }

        if (md->objecttype != objectType)
        {
            PyErr_Format(SaiRedisError, "Attribute: %s is not %s", kvp.first.c_str(), info->objecttypename);
            return nullptr;
        }

        sai_attribute_t attr;

        try
        {
            attr.id = md->attrid;

            sai_deserialize_attr_value(kvp.second, *md, attr, false);
        }
        catch (const std::exception&e)
        {
            PyErr_Format(SaiRedisError, "Failed to deserialize %s '%s': %s", kvp.first.c_str(), kvp.second.c_str(), e.what());
            return nullptr;
        }

        attrs.push_back(attr);
        meta.push_back(md);

    }

    // call actual function create !

    sai_object_id_t objectId;
    sai_status_t status = g_sai->create(
            objectType,
            &objectId,
            switchId,
            (uint32_t)attrs.size(),
            attrs.data());

    for (size_t i = 0; i < attrs.size(); i++)
    {
        // free potentially allocated memory
        sai_deserialize_free_attribute_value(meta[i]->attrvaluetype, attrs[i]);
    }

    PyObject *pdict = PyDict_New();
    PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

    if (status == SAI_STATUS_SUCCESS)
    {
        PyDict_SetItemString(pdict, "oid", PyString_FromFormat("%s", sai_serialize_object_id(objectId).c_str()));
    }

    return pdict;
}

static PyObject * generic_remove(
        _In_ sai_object_type_t objectType,
        _In_ PyObject *self, 
        _In_ PyObject *args)
{
    SWSS_LOG_ENTER();

    auto* info = sai_metadata_get_object_type_info(objectType);

    if (!info)
    {
        PyErr_Format(SaiRedisError, "Invalid object type specified");
        return nullptr;
    }

    if (info->isnonobjectid)
    {
        PyErr_Format(SaiRedisError, "Non object id specified to oid function");
        return nullptr;
    }

    if (!PyTuple_Check(args))
    {
        PyErr_Format(SaiRedisError, "Python error, expected args type is tuple");
        return nullptr;
    }

    int size = (int)PyTuple_Size(args);

    if (size != 1)
    {
        PyErr_Format(SaiRedisError, "Expected number of arguments is 1, but %d given", size);
        return nullptr;
    }

    auto*oid = PyTuple_GetItem(args, 0);

    if (!PyString_Check(oid))
    {
        PyErr_Format(SaiRedisError, "Passed argument must be of type string");
        return nullptr;
    }

    sai_object_id_t objectId;

    try
    {
        sai_deserialize_object_id(PyString_AsString(oid), &objectId);
    }
    catch (const std::exception&e)
    {
        PyErr_Format(SaiRedisError, "Failed to deserialize switchId: %s", e.what());
        return nullptr;
    }

    sai_status_t status = g_sai->remove(
            objectType,
            objectId);

    PyObject *pdict = PyDict_New();
    PyDict_SetItemString(pdict, "status", PyString_FromFormat("%s", sai_serialize_status(status).c_str()));

    return pdict;
}

// CREATE

static PyObject* create_switch(PyObject *self, PyObject *args)
{
    SWSS_LOG_ENTER();

    return generic_create(SAI_OBJECT_TYPE_SWITCH, self, args);
}

static PyObject* create_vlan(PyObject *self, PyObject *args)
{
    SWSS_LOG_ENTER();

    return generic_create(SAI_OBJECT_TYPE_VLAN, self, args);
}

// REMOVE

static PyObject* remove_switch(PyObject *self, PyObject *args)
{
    SWSS_LOG_ENTER();

    return generic_remove(SAI_OBJECT_TYPE_SWITCH, self, args);
}

static PyObject* remove_vlan(PyObject *self, PyObject *args)
{
    SWSS_LOG_ENTER();

    return generic_remove(SAI_OBJECT_TYPE_VLAN, self, args);
}
