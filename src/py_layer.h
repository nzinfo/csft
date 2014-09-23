#ifndef _PY_LAYER_H_
#define _PY_LAYER_H_
#include "sphinx.h"

#if USE_PYTHON

#if USE_PYTHON_DEBUG	
	#include   <Python.h>    
#else
	#ifdef _DEBUG
		#define D_E_B_U_G
		#undef   _DEBUG
	#endif
	#include   <Python.h>    
	#ifdef	D_E_B_U_G
		#undef  D_E_B_U_G
		#define _DEBUG
	#endif

#endif //USE_PYTHON_DEBUG

//////////////////////////////////////////////////////////////////////////

bool			cftInitialize( const CSphConfigSection & hPython);
void			cftShutdown();

bool SpawnSourcePython ( const CSphConfigSection & hSource, const char * sSourceName, CSphSource** pSrcPython);

#endif //USE_PYTHON

#endif

