#ifndef __PYX_HAVE_API__py_csft
#define __PYX_HAVE_API__py_csft
#include "Python.h"
#include "py_csft.h"

static void (*__pyx_f_7py_csft___setPythonPath)(char const *) = 0;
#define __setPythonPath __pyx_f_7py_csft___setPythonPath
static PyObject *(*__pyx_f_7py_csft___getPythonClassByName)(char const *) = 0;
#define __getPythonClassByName __pyx_f_7py_csft___getPythonClassByName
static CSphSource *(*__pyx_f_7py_csft_createPythonDataSourceObject)(char const *, char const *) = 0;
#define createPythonDataSourceObject __pyx_f_7py_csft_createPythonDataSourceObject
#if !defined(__Pyx_PyIdentifier_FromString)
#if PY_MAJOR_VERSION < 3
  #define __Pyx_PyIdentifier_FromString(s) PyString_FromString(s)
#else
  #define __Pyx_PyIdentifier_FromString(s) PyUnicode_FromString(s)
#endif
#endif

#ifndef __PYX_HAVE_RT_ImportModule
#define __PYX_HAVE_RT_ImportModule
static PyObject *__Pyx_ImportModule(const char *name) {
    PyObject *py_name = 0;
    PyObject *py_module = 0;
    py_name = __Pyx_PyIdentifier_FromString(name);
    if (!py_name)
        goto bad;
    py_module = PyImport_Import(py_name);
    Py_DECREF(py_name);
    return py_module;
bad:
    Py_XDECREF(py_name);
    return 0;
}
#endif

#ifndef __PYX_HAVE_RT_ImportFunction
#define __PYX_HAVE_RT_ImportFunction
static int __Pyx_ImportFunction(PyObject *module, const char *funcname, void (**f)(void), const char *sig) {
    PyObject *d = 0;
    PyObject *cobj = 0;
    union {
        void (*fp)(void);
        void *p;
    } tmp;
    d = PyObject_GetAttrString(module, (char *)"__pyx_capi__");
    if (!d)
        goto bad;
    cobj = PyDict_GetItemString(d, funcname);
    if (!cobj) {
        PyErr_Format(PyExc_ImportError,
            "%.200s does not export expected C function %.200s",
                PyModule_GetName(module), funcname);
        goto bad;
    }
#if PY_VERSION_HEX >= 0x02070000 && !(PY_MAJOR_VERSION==3 && PY_MINOR_VERSION==0)
    if (!PyCapsule_IsValid(cobj, sig)) {
        PyErr_Format(PyExc_TypeError,
            "C function %.200s.%.200s has wrong signature (expected %.500s, got %.500s)",
             PyModule_GetName(module), funcname, sig, PyCapsule_GetName(cobj));
        goto bad;
    }
    tmp.p = PyCapsule_GetPointer(cobj, sig);
#else
    {const char *desc, *s1, *s2;
    desc = (const char *)PyCObject_GetDesc(cobj);
    if (!desc)
        goto bad;
    s1 = desc; s2 = sig;
    while (*s1 != '\0' && *s1 == *s2) { s1++; s2++; }
    if (*s1 != *s2) {
        PyErr_Format(PyExc_TypeError,
            "C function %.200s.%.200s has wrong signature (expected %.500s, got %.500s)",
             PyModule_GetName(module), funcname, sig, desc);
        goto bad;
    }
    tmp.p = PyCObject_AsVoidPtr(cobj);}
#endif
    *f = tmp.fp;
    if (!(*f))
        goto bad;
    Py_DECREF(d);
    return 0;
bad:
    Py_XDECREF(d);
    return -1;
}
#endif


static int import_py_csft(void) {
  PyObject *module = 0;
  module = __Pyx_ImportModule("py_csft");
  if (!module) goto bad;
  if (__Pyx_ImportFunction(module, "__setPythonPath", (void (**)(void))&__pyx_f_7py_csft___setPythonPath, "void (char const *)") < 0) goto bad;
  if (__Pyx_ImportFunction(module, "__getPythonClassByName", (void (**)(void))&__pyx_f_7py_csft___getPythonClassByName, "PyObject *(char const *)") < 0) goto bad;
  if (__Pyx_ImportFunction(module, "createPythonDataSourceObject", (void (**)(void))&__pyx_f_7py_csft_createPythonDataSourceObject, "CSphSource *(char const *, char const *)") < 0) goto bad;
  Py_DECREF(module); module = 0;
  return 0;
  bad:
  Py_XDECREF(module);
  return -1;
}

#endif /* !__PYX_HAVE_API__py_csft */
