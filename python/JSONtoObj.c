#include <Python.h>
#include <ultrajson.h>




void Object_objectAddKey(JSOBJ obj, JSOBJ name, JSOBJ value)
{
	PyDict_SetItem (obj, name, value);
	Py_DECREF( (PyObject *) name);
	Py_DECREF( (PyObject *) value);
	return;
}

void Object_arrayAddItem(JSOBJ obj, JSOBJ value)
{
	PyList_Append(obj, value);
	Py_DECREF( (PyObject *) value);
	return;
}

JSOBJ Object_newString(char *start, char *end)
{
	return PyString_FromStringAndSize(start, (end - start));
}

JSOBJ Object_newTrue(void)
{ 
	Py_RETURN_TRUE;
}

JSOBJ Object_newFalse(void)
{
	Py_RETURN_FALSE;
}

JSOBJ Object_newNull(void)
{
	Py_RETURN_NONE;
}

JSOBJ Object_newObject(void)
{
	return PyDict_New();
}

JSOBJ Object_newArray(void)
{
	return PyList_New(0);
}

JSOBJ Object_newInteger(JSINT32 value)
{
	return PyInt_FromLong( (long) value);
}

JSOBJ Object_newLong(JSINT64 value)
{
	return PyLong_FromLongLong (value);
}

JSOBJ Object_newDouble(double value)
{ 
	return PyFloat_FromDouble(value);
}

static void Object_releaseObject(JSOBJ obj)
{
	Py_DECREF( ((PyObject *)obj));
}



PyObject* JSONToObj(PyObject* self, PyObject *arg)
{
	PyObject *ret;
	JSONObjectDecoder decoder = 
	{
		Object_newString,
		Object_objectAddKey,
		Object_arrayAddItem,
		Object_newTrue,
		Object_newFalse,
		Object_newNull,
		Object_newObject,
		Object_newArray,
		Object_newInteger,
		Object_newLong,
		Object_newDouble,
		Object_releaseObject,
		PyObject_Malloc,
		PyObject_Free,
		PyObject_Realloc
	};

	if (!PyString_Check(arg))
	{
		PyErr_Format(PyExc_TypeError, "Expected string");
		return NULL;
	}

	decoder.errorStr = NULL;
	decoder.errorOffset = NULL;
	
	ret = JSON_DecodeObject(&decoder, PyString_AS_STRING(arg), PyString_GET_SIZE(arg)); 

	if (decoder.errorStr)
	{
		/*
		FIXME: It's possible to give a much nicer error message here with actual failing element in input etc*/
		
		PyErr_Format (PyExc_ValueError, "%s", decoder.errorStr);
		
		if (ret)
		{
			Py_DECREF( (PyObject *) ret);
		}
		
		return NULL;
	}
	
	return ret;
}
