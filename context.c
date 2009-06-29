#include "context.h"
#include "object.h"
#include "utils.h"

static void
PYM_JSContextDealloc(PYM_JSContextObject *self)
{
  if (self->cx) {
    JS_DestroyContext(self->cx);
    self->cx = NULL;
  }

  Py_DECREF(self->runtime);
  self->runtime = NULL;

  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
PYM_getRuntime(PYM_JSContextObject *self, PyObject *args)
{
  Py_INCREF(self->runtime);
  return (PyObject *) self->runtime;
}

static PyObject *
PYM_newObject(PYM_JSContextObject *self, PyObject *args)
{
  PYM_JSObject *object = PyObject_New(PYM_JSObject,
                                      &PYM_JSObjectType);
  if (object == NULL)
    return NULL;

  object->runtime = self->runtime;
  Py_INCREF(object->runtime);

  object->obj = JS_NewObject(self->cx, &PYM_JS_ObjectClass, NULL, NULL);
  if (object->obj == NULL) {
    PyErr_SetString(PYM_error, "JS_NewObject() failed");
    Py_DECREF(object);
    return NULL;
  }

  JS_AddNamedRootRT(object->runtime->rt, &object->obj,
                    "Pymonkey-Generated Object");

  return (PyObject *) object;
}

static PyObject *
PYM_getProperty(PYM_JSContextObject *self, PyObject *args)
{
  // TODO: We're making a lot of copies of the string here, which
  // can't be very efficient.

  PYM_JSObject *object;
  char *string;

  if (!JS_CStringsAreUTF8()) {
    PyErr_SetString(PyExc_NotImplementedError,
                    "Data type conversion not implemented.");
    return NULL;
  }

  if (!PyArg_ParseTuple(args, "O!es", &PYM_JSObjectType, &object,
                        "utf-8", &string))
    return NULL;

  JSString *jsString = JS_NewStringCopyZ(self->cx, string);
  if (jsString == NULL) {
    PyMem_Free(string);
    PyErr_SetString(PYM_error, "JS_NewStringCopyZ() failed");
    return NULL;
  }

  jsval val;
  if (!JS_GetUCProperty(self->cx, object->obj,
                        JS_GetStringChars(jsString),
                        JS_GetStringLength(jsString), &val)) {
    // TODO: Get the actual JS exception. Any exception that exists
    // here will probably still be pending on the JS context.
    PyMem_Free(string);
    PyErr_SetString(PYM_error, "Getting property failed.");
    return NULL;
  }

  PyMem_Free(string);
  return PYM_jsvalToPyObject(val);
}

static PyObject *
PYM_initStandardClasses(PYM_JSContextObject *self, PyObject *args)
{
  PYM_JSObject *object;

  if (!PyArg_ParseTuple(args, "O!", &PYM_JSObjectType, &object))
    return NULL;

  if (!JS_InitStandardClasses(self->cx, object->obj)) {
    PyErr_SetString(PYM_error, "JS_InitStandardClasses() failed");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PYM_evaluateScript(PYM_JSContextObject *self, PyObject *args)
{
  PYM_JSObject *object;
  const char *source;
  int sourceLen;
  const char *filename;
  int lineNo;

  if (!PyArg_ParseTuple(args, "O!s#si", &PYM_JSObjectType, &object,
                        &source, &sourceLen, &filename, &lineNo))
    return NULL;

  JS_BeginRequest(self->cx);

  jsval rval;
  if (!JS_EvaluateScript(self->cx, object->obj, source, sourceLen,
                         filename, lineNo, &rval)) {
    // TODO: Actually get the error that was raised.
    PyErr_SetString(PYM_error, "Script failed");
    JS_EndRequest(self->cx);
    return NULL;
  }

  PyObject *pyRval = PYM_jsvalToPyObject(rval);

  JS_EndRequest(self->cx);

  return pyRval;
}

static PyMethodDef PYM_JSContextMethods[] = {
  {"get_runtime", (PyCFunction) PYM_getRuntime, METH_VARARGS,
   "Get the JavaScript runtime associated with this context."},
  {"new_object", (PyCFunction) PYM_newObject, METH_VARARGS,
   "Create a new JavaScript object."},
  {"init_standard_classes",
   (PyCFunction) PYM_initStandardClasses, METH_VARARGS,
   "Add standard classes and functions to the given object."},
  {"evaluate_script",
   (PyCFunction) PYM_evaluateScript, METH_VARARGS,
   "Evaluate the given JavaScript code in the context of the given "
   "global object, using the given filename"
   "and line number information."},
  {"get_property", (PyCFunction) PYM_getProperty, METH_VARARGS,
   "Gets the given property for the given JavaScript object."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject PYM_JSContextType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Context",          /*tp_name*/
  sizeof(PYM_JSContextObject), /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  /*tp_dealloc*/
  (destructor) PYM_JSContextDealloc,
  0,                           /*tp_print*/
  0,                           /*tp_getattr*/
  0,                           /*tp_setattr*/
  0,                           /*tp_compare*/
  0,                           /*tp_repr*/
  0,                           /*tp_as_number*/
  0,                           /*tp_as_sequence*/
  0,                           /*tp_as_mapping*/
  0,                           /*tp_hash */
  0,                           /*tp_call*/
  0,                           /*tp_str*/
  0,                           /*tp_getattro*/
  0,                           /*tp_setattro*/
  0,                           /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,          /*tp_flags*/
  /* tp_doc */
  "JavaScript Context.",
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  PYM_JSContextMethods,        /* tp_methods */
  0,                           /* tp_members */
  0,                           /* tp_getset */
  0,                           /* tp_base */
  0,                           /* tp_dict */
  0,                           /* tp_descr_get */
  0,                           /* tp_descr_set */
  0,                           /* tp_dictoffset */
  0,                           /* tp_init */
  0,                           /* tp_alloc */
  0,                           /* tp_new */
};
