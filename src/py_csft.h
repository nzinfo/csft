#ifndef __PYX_HAVE__py_csft
#define __PYX_HAVE__py_csft


#ifndef __PYX_HAVE_API__py_csft

#ifndef __PYX_EXTERN_C
  #ifdef __cplusplus
    #define __PYX_EXTERN_C extern "C"
  #else
    #define __PYX_EXTERN_C extern
  #endif
#endif

__PYX_EXTERN_C DL_IMPORT(void) __setPythonPath(char const *);
__PYX_EXTERN_C DL_IMPORT(PyObject) *__getPythonClassByName(char const *);
__PYX_EXTERN_C DL_IMPORT(int) py_source_setup(void *, CSphConfigSection const &);
__PYX_EXTERN_C DL_IMPORT(int) py_source_connected(void *, CSphSchema &);
__PYX_EXTERN_C DL_IMPORT(int) py_source_index_finished(void *);
__PYX_EXTERN_C DL_IMPORT(int) py_source_before_index(void *);
__PYX_EXTERN_C DL_IMPORT(int) py_source_get_join_field(void *, char const *);
__PYX_EXTERN_C DL_IMPORT(int) py_source_get_join_mva(void *, char const *, uint64_t *, int64_t *);
__PYX_EXTERN_C DL_IMPORT(int) py_source_next(void *);
__PYX_EXTERN_C DL_IMPORT(int) py_source_after_index(void *, bool);
__PYX_EXTERN_C DL_IMPORT(int) py_source_get_kill_list(void *);
__PYX_EXTERN_C DL_IMPORT(int) py_source_get_kill_list_item(void *, uint64_t *);
__PYX_EXTERN_C DL_IMPORT(CSphSource) *createPythonDataSourceObject(char const *, char const *);

#endif /* !__PYX_HAVE_API__py_csft */

#if PY_MAJOR_VERSION < 3
PyMODINIT_FUNC initpy_csft(void);
#else
PyMODINIT_FUNC PyInit_py_csft(void);
#endif

#endif /* !__PYX_HAVE__py_csft */
