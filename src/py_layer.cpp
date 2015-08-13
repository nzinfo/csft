#ifdef _WIN32
#pragma warning(disable:4996) 
#pragma warning(disable:4127) 
#endif

#include "sphinx.h"
#include "sphinxutils.h"

#include "py_layer.h"
#include "py_source.h"
#include "py_csft.h"

#if USE_PYTHON

#define LOC_CHECK(_hash,_key,_msg,_add) \
    if (!( _hash.Exists ( _key ) )) \
    { \
        fprintf ( stdout, "ERROR: key '%s' not found " _msg "\n", _key, _add ); \
        return false; \
    }

bool SpawnSourcePython ( const CSphConfigSection & hSource, const char * sSourceName, CSphSource** pSrcPython)
{
    assert ( hSource["type"]=="python_legacy" );

	LOC_CHECK ( hSource, "name", "in source '%s'.", sSourceName );
	
    * pSrcPython = NULL;

	CSphSource_Python * pPySource = new CSphSource_Python ( sSourceName );
	if ( !pPySource->Setup ( hSource ) ) {
		if(pPySource->m_sError.Length())
			fprintf ( stdout, "ERROR: %s\n", pPySource->m_sError.cstr());
		SafeDelete ( pPySource );
	}

    * pSrcPython = pPySource;

    return true;
}

//////////////////////////////////////////////////////////////////////////
// get array of strings
#define LOC_GETAS(_sec, _arg,_key) \
	for ( CSphVariant * pVal = _sec(_key); pVal; pVal = pVal->m_pNext ) \
	_arg.Add ( pVal->cstr() );

// helper functions
#if USE_PYTHON

//FIXME: change to cython version..
int init_python_layer_helpers()
{
	int nRet = 0;
	nRet = PyRun_SimpleString("import sys\nimport os\n");
	if(nRet) return nRet;
	//helper function to append path to env.
	nRet = PyRun_SimpleString("\n\
def __coreseek_set_python_path(sPath):\n\
    sPaths = [x.lower() for x in sys.path]\n\
    sPath = os.path.abspath(sPath)\n\
    if sPath not in sPaths:\n\
        sys.path.append(sPath)\n\
    #print sPaths\n\
\n");
	if(nRet) return nRet;
	// helper function to find data source
	nRet = PyRun_SimpleString("\n\
def __coreseek_find_pysource(sName): \n\
    pos = sName.rfind('.') \n\
    module_name = sName[:pos]\n\
    try:\n\
        exec ('%s=__import__(\"%s\")' % (module_name, module_name))\n\
        return eval(sName)\n\
    except ImportError, e:\n\
		print e\n\
		return None\n\
\n");
	return nRet;
}

#endif

bool	cftInitialize( const CSphConfigSection & hPython)
{
#if USE_PYTHON
    CSphString progName = "csft";
    Py_SetProgramName((char*)progName.cstr());

    if (!Py_IsInitialized()) {
        Py_InitializeEx(0);
        //PyEval_InitThreads();

        if (!Py_IsInitialized()) {
            return false;
        }
        // init extension.
        initpy_csft();
        // init legacy helper
        init_python_layer_helpers();
    }

    // init paths
    {

        CSphVector<CSphString>	m_dPyPaths;
        LOC_GETAS(hPython, m_dPyPaths, "python_path");
        ///XXX: append system pre-defined path here.
        ARRAY_FOREACH ( i, m_dPyPaths )
        {
           __setPythonPath( m_dPyPaths[i].cstr() );
        }
    }
    // check the demo[debug] object creation.
    if( hPython("__debug_object_class") )
    {
        CSphString demoClassName = hPython.GetStr ( "__debug_object_class" );
        PyObject* m_pTypeObj = __getPythonClassByName(demoClassName.cstr());
        printf("The python type object's address %p .\n", m_pTypeObj);
    }
#endif
	return true;
}

void			cftShutdown()
{

#if USE_PYTHON
		//FIXME: avoid the debug warning.
		if (Py_IsInitialized()) {
				//to avoid crash in release mode.
				Py_Finalize();
		}

#endif
}

#endif //USE_PYTHON

