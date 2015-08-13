#ifdef _WIN32
#pragma warning(disable:4996) 
#pragma warning(disable:4127) 
#endif

#include "sphinx.h"
#include "sphinxutils.h"
//#include "sphinx_internal.h"
#include "py_source.h"
#include "sphinxsearch.h"


//py-layer HITMAN, -> change it if sphinx.cpp changed.
typedef Hitman_c<8> HITMAN;

//just copy from sphinx.cpp
#define SPH_INTERNAL_PROFILER 0

#if SPH_INTERNAL_PROFILER

enum ESphTimer
{
	TIMER_root = 0,

#define DECLARE_TIMER(_arg) TIMER_##_arg,
#include "sphinxtimers.h"
#undef DECLARE_TIMER

	TIMERS_TOTAL
};


static const char * const g_dTimerNames [ TIMERS_TOTAL ] =
{
	"root",

#define DECLARE_TIMER(_arg) #_arg,
#include "sphinxtimers.h"
#undef DECLARE_TIMER
};


struct CSphTimer
{
	int64_t			m_iMicroSec;
	ESphTimer		m_eTimer;
	int				m_iParent;
	int				m_iChild;
	int				m_iNext;
	int				m_iPrev;
	int				m_iCalls;

	CSphTimer ()
	{
		Alloc ( TIMER_root, -1 );
	}

	void Alloc ( ESphTimer eTimer, int iParent )
	{
		m_iParent = iParent;
		m_iChild = -1;
		m_iNext = -1;
		m_iPrev = -1;
		m_eTimer = eTimer;
		m_iMicroSec = 0;
		m_iCalls = 0;
	}

	void Start ()
	{
		m_iMicroSec -= sphMicroTimer ();
		m_iCalls++;
	}

	void Stop ()
	{
		m_iMicroSec += sphMicroTimer ();
	}
};

static const int	SPH_MAX_TIMERS					= 128;
static int			g_iTimer						= -1;
static int			g_iTimers						= 0;
static CSphTimer	g_dTimers [ SPH_MAX_TIMERS ];


void sphProfilerInit ()
{
	assert ( g_iTimers==0 );
	assert ( g_iTimer==-1 );

	// start root timer
	g_iTimers = 1;
	g_iTimer = 0;
	g_dTimers[g_iTimer].Alloc ( TIMER_root, -1 );
	g_dTimers[g_iTimer].Start ();
}


void sphProfilerPush ( ESphTimer eTimer )
{
	assert ( g_iTimer>=0 && g_iTimer<SPH_MAX_TIMERS );
	assert ( eTimer!=TIMER_root );

	// search for match timer in current timer's children list
	int iTimer;
	for ( iTimer=g_dTimers[g_iTimer].m_iChild;
		iTimer>0;
		iTimer=g_dTimers[iTimer].m_iNext )
	{
		if ( g_dTimers[iTimer].m_eTimer==eTimer )
			break;
	}

	// not found? let's alloc
	if ( iTimer<0 )
	{
		assert ( g_iTimers<SPH_MAX_TIMERS );
		iTimer = g_iTimers++;

		// create child and make current timer it's parent
		g_dTimers[iTimer].Alloc ( eTimer, g_iTimer );

		// make it new children list head
		g_dTimers[iTimer].m_iNext = g_dTimers[g_iTimer].m_iChild;
		if ( g_dTimers[g_iTimer].m_iChild>=0 )
			g_dTimers [ g_dTimers[g_iTimer].m_iChild ].m_iPrev = iTimer;
		g_dTimers[g_iTimer].m_iChild = iTimer;
	}

	// make it new current one
	assert ( iTimer>0 );
	g_dTimers[iTimer].Start ();
	g_iTimer = iTimer;
}


void sphProfilerPop ( ESphTimer eTimer )
{
	assert ( g_iTimer>0 && g_iTimer<SPH_MAX_TIMERS );
	assert ( g_dTimers[g_iTimer].m_eTimer==eTimer );

	g_dTimers[g_iTimer].Stop ();
	g_iTimer = g_dTimers[g_iTimer].m_iParent;
	assert ( g_iTimer>=0 && g_iTimer<SPH_MAX_TIMERS );
}


void sphProfilerDone ()
{
	assert ( g_iTimers>0 );
	assert ( g_iTimer==0 );

	// stop root timer
	g_iTimers = 0;
	g_iTimer = -1;
	g_dTimers[0].Stop ();
}


void sphProfilerShow ( int iTimer=0, int iLevel=0 )
{
	assert ( g_iTimers==0 );
	assert ( g_iTimer==-1 );

	if ( iTimer==0 )
		fprintf ( stdout, "--- PROFILE ---\n" );

	CSphTimer & tTimer = g_dTimers[iTimer];
	int iChild;

	// calc me
	int iChildren = 0;
	int64_t tmSelf = tTimer.m_iMicroSec;
	for ( iChild=tTimer.m_iChild; iChild>0; iChild=g_dTimers[iChild].m_iNext, iChildren++ )
		tmSelf -= g_dTimers[iChild].m_iMicroSec;

	// dump me
	if ( tTimer.m_iMicroSec<50 )
		return;

	char sName[32];
	for ( int i=0; i<iLevel; i++ )
		sName[2*i] = sName[2*i+1] = ' ';
	sName[2*iLevel] = '\0';
	strncat ( sName, g_dTimerNames [ tTimer.m_eTimer ], sizeof(sName) );

	fprintf ( stdout, "%-32s | %6d.%02d ms | %6d.%02d ms self | %d calls\n",
		sName,
		int(tTimer.m_iMicroSec/1000), int(tTimer.m_iMicroSec%1000)/10,
		int(tmSelf/1000), int(tmSelf%1000)/10,
		tTimer.m_iCalls );

	// dump my children
	iChild = tTimer.m_iChild;
	while ( iChild>0 && g_dTimers[iChild].m_iNext>0 )
		iChild = g_dTimers[iChild].m_iNext;

	while ( iChild>0 )
	{
		sphProfilerShow ( iChild, 1+iLevel );
		iChild = g_dTimers[iChild].m_iPrev;
	}

	if ( iTimer==0 )
		fprintf ( stdout, "---------------\n" );
}


class CSphEasyTimer
{
public:
	CSphEasyTimer ( ESphTimer eTimer )
		: m_eTimer ( eTimer )
	{
		if ( g_iTimer>=0 )
			sphProfilerPush ( m_eTimer );
	}

	~CSphEasyTimer ()
	{
		if ( g_iTimer>=0 )
			sphProfilerPop ( m_eTimer );
	}

protected:
	ESphTimer		m_eTimer;
};


#define PROFILER_INIT() sphProfilerInit()
#define PROFILER_DONE() sphProfilerDone()
#define PROFILE_BEGIN(_arg) sphProfilerPush(TIMER_##_arg)
#define PROFILE_END(_arg) sphProfilerPop(TIMER_##_arg)
#define PROFILE_SHOW() sphProfilerShow()
#define PROFILE(_arg) CSphEasyTimer __t_##_arg ( TIMER_##_arg );

#else

#define PROFILER_INIT()
#define PROFILER_DONE()
#define PROFILE_BEGIN(_arg)
#define PROFILE_END(_arg)
#define PROFILE_SHOW()
#define PROFILE(_arg)

#endif // SPH_INTERNAL_PROFILER


#if USE_PYTHON


#define LOC_ERROR2(_msg,_arg,_arg2)		{ sError.SetSprintf ( _msg, _arg, _arg2 ); return false; }

PyObject* GetObjectAttr(PyObject *pInst, char* name);

CSphSource_Python::CSphSource_Python ( const char * sName )
				 : CSphSource_Document ( sName )
				 , m_JoinFieldsResultPos( 0 )
				 , m_uMaxFetchedID(0)
				 , m_iJoinedHitID( 0 )
				 , m_sError()
				 , m_iKillListSize( 0 )
				 , m_iKillListPos( 0 )
				 , m_iJoinedHitField( 0 )
{
	//sName the data source's name
	main_module = NULL;
	builtin_module = NULL;
	m_pInstance = NULL;
	m_pInstance_BuildHit = NULL;
	m_pInstance_NextDocument = NULL;
	m_pInstance_GetDocField = NULL;
	m_pInstance_GetMVAValue = NULL;

	m_bHaveCheckBuildHit = false;
	m_JoinFieldsResult = NULL;

	for(int i = 0; i< SPH_MAX_FIELDS; i++){
		m_dFields[i] = NULL;
		m_iJoinedHitPositions[i] = 0;
	}

	m_pKillList = NULL;
}

CSphSource_Python::~CSphSource_Python (){
	Disconnect ();
	
	if(m_pInstance_BuildHit) {
		Py_XDECREF(m_pInstance_BuildHit);
		m_pInstance_BuildHit = NULL;
	}

	if(m_pInstance_NextDocument){
		Py_XDECREF(m_pInstance_BuildHit);
		m_pInstance_BuildHit = NULL;
	}

	if(m_pInstance_GetDocField){
		Py_XDECREF(m_pInstance_GetDocField);
		m_pInstance_GetDocField = NULL;
	}

	if(m_pInstance_GetMVAValue){
		Py_XDECREF(m_pInstance_GetMVAValue);
		m_pInstance_GetMVAValue = NULL;
	}

	if(m_JoinFieldsResult){
		Py_XDECREF(m_JoinFieldsResult);
		m_JoinFieldsResult = NULL;
	}
	
	//can not be in Disconnect for OnIndexFinished still needs this.
	if(m_pInstance) {
		Py_XDECREF(m_pInstance);
		m_pInstance = NULL;
	}
}

// get string
#define LOC_GETS(_arg,_key) \
	if ( hSource.Exists(_key) ) \
		_arg = hSource[_key].strval();

// get array of strings
#define LOC_GETAS(_arg,_key) \
	for ( CSphVariant * pVal = hSource(_key); pVal; pVal = pVal->m_pNext ) \
		_arg.Add ( pVal->cstr() );

bool	CSphSource_Python::Setup ( const CSphConfigSection & hSource){

	//check have python_name?
	CSphString	PySourceName;
	LOC_GETS(PySourceName, "name");
	if(PySourceName.Length() == 0) {
		Error ( "field 'name' must be assigned.");
		return false;
	}

	//Shame on me!! __main__ __builtin__ time no Refcnt +1
	main_module = PyImport_AddModule("__main__");  
	builtin_module =  PyImport_AddModule("__builtin__"); 
	
	//do schema read, ready to used.
	if (!main_module) { goto DONE; }
	if (!builtin_module) { 
		//Py_XDECREF(main_module);
		goto DONE; 
	}

	if(InitDataSchema(hSource, PySourceName.cstr())!=0) 
		return false;

	return true;
DONE:
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Helper Functions
//////////////////////////////////////////////////////////////////////////
bool CheckResult(PyObject * pResult)
{
	if(!pResult) //no return default true
		return true;

	if(PyBool_Check(pResult)){
		if(Py_False == pResult)			
			return false;
	}
	if(PyInt_Check(pResult)) {
		if(PyInt_AsLong(pResult) == 0)
			return false;
	}
	return true;
}

int CSphSource_Python::UpdatePySchema( PyObject * pList, CSphSchema * pInfo,  CSphString & docid, CSphString & sError )
{
	/*
		- Update pInfo via pList.
		- Most copy from pysource, for no pysource needed in pyExt mode.
		@return 0, ok
		@return <0, error happend.
	*/
	assert(pInfo);
	pInfo->Reset();
	char* doc_id_col = NULL; //used to save docid.

	//
	if(!pList || !PyList_Check(pList)) {
		sError = "Feed list object to schema.";
		return -1;
	}

	int size = (int)PyList_Size(pList);
	for(int i = 0; i < size; i++) {
		PyObject* item = PyList_GetItem(pList,i);
		
		// item -> tuple ( name, {props} )
		if(!PyTuple_Check(item)){
			sError = "The schema list must be build by tuples.";
			return -2;
		}

		if(PyTuple_GET_SIZE(item) < 2) {
			// name, props
			return -3;
		}
		
		PyObject* key = PyTuple_GetItem(item, 0);
		PyObject* props = PyTuple_GetItem(item, 1);
		//check type
		if(PyString_Check(key) && PyDict_Check(props))
		{
			char* strkey = PyString_AsString(key);
			CSphString skey(strkey); 
			
			// Currently support case-sensive column while indexing
			//check key
#if USE_PYTHON_CASE_SENSIVE_ATTR
#else
			{
				CSphString skey_low = skey;
				skey_low.ToLower();
				if(skey_low != skey) {
					sError.SetSprintf("The schema column name must be in lower case [%s]", skey.cstr());
					return -3;
				}
			}
#endif

			//check docid
			PyObject * propValue = NULL;
			PyObject * sizeValue = NULL;
			PyObject * wordPartValue = NULL;
			PyObject * delayCollectionValue = NULL;

			int	iBitCount = -1;
			propValue = PyDict_GetItemString(props, "docid"); //+0
			if(propValue && CheckResult(propValue)){
				doc_id_col = strkey;
				continue;
			}

			propValue = PyDict_GetItemString(props, "type"); 
			if (propValue && !PyString_Check(propValue)) {
				sError.SetSprintf( "Attribute %s's type is not a string value", strkey);
				continue;
			}

			// set tCol.m_tLocator.m_iBitCount, to make index smaller
			sizeValue = PyDict_GetItemString(props, "size");
			if(sizeValue && (PyInt_Check(sizeValue))) {
				//PyErr_Print();
				iBitCount = PyInt_AsLong(sizeValue);
			}
			if(sizeValue && (PyLong_Check(sizeValue))) {
				//PyErr_Print();
				iBitCount = (int)PyLong_AsLongLong(sizeValue);
			}
		
			wordPartValue = PyDict_GetItemString(props, "wordpart"); //+0
			if (wordPartValue){
				if (PyString_Check(wordPartValue)) {
					CSphString szFieldName(strkey);
					char* strval = PyString_AsString(wordPartValue);

					if(strcmp(strval,"prefix") == 0)  {
						if(!m_dPrefixFields.Contains ( strkey ))
							m_dPrefixFields.AddUnique(strkey);
					}
					if(strcmp(strval,"infix") == 0 )  {
						if(!m_dInfixFields.Contains ( strkey ))
							m_dInfixFields.AddUnique(strkey);
					}
					
				}else {
					sError.SetSprintf( "Attribute %s's type is not a string value, skip", "wordpart");
					wordPartValue = NULL;
				}
			}

			delayCollectionValue = PyDict_GetItemString(props, "delay"); //+0
			if(delayCollectionValue && CheckResult(delayCollectionValue)){
				//doc_id_col = strkey;
				//continue;
			}

			// assign types
			if(strcmp(PyString_AsString(propValue) , "float") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_FLOAT );
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				tCol.m_tLocator.m_iBitCount = iBitCount;
				pInfo->AddAttr ( tCol, true);
			}else
			if(strcmp(PyString_AsString(propValue) , "integer") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_INTEGER );
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				tCol.m_tLocator.m_iBitCount = iBitCount;
				pInfo->AddAttr ( tCol, true);
			}else
			if(strcmp(PyString_AsString(propValue) , "long") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_BIGINT );
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				tCol.m_tLocator.m_iBitCount = iBitCount;
				pInfo->AddAttr ( tCol, true);
			}else
			if(strcmp(PyString_AsString(propValue) , "list") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_INTEGER );
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				tCol.m_eAttrType = SPH_ATTR_UINT32SET;
				tCol.m_eSrc = SPH_ATTRSRC_FIELD;
				// tCol.m_tLocator.m_iBitCount = iBitCount; //XXX: ????
				pInfo->AddAttr ( tCol, true);
			}else
			if(strcmp(PyString_AsString(propValue) , "list64") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_INT64SET ); 
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				tCol.m_eAttrType = SPH_ATTR_INT64SET;
				tCol.m_eSrc = SPH_ATTRSRC_FIELD;
				// tCol.m_tLocator.m_iBitCount = iBitCount; //XXX: ????
				pInfo->AddAttr ( tCol, true);
			}else
			if(strcmp(PyString_AsString(propValue) , "list-query") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_INTEGER );
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				tCol.m_eAttrType = SPH_ATTR_UINT32SET;
				tCol.m_eSrc = SPH_ATTRSRC_QUERY;
				// tCol.m_tLocator.m_iBitCount = iBitCount; //XXX: ????
				pInfo->AddAttr ( tCol, true);
			}else
			if(strcmp(PyString_AsString(propValue) , "bool") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_BOOL );
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				pInfo->AddAttr ( tCol, true );
			}else
			if(strcmp(PyString_AsString(propValue) , "timestamp") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_TIMESTAMP );
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				pInfo->AddAttr ( tCol, true );
			}else
			if(strcmp(PyString_AsString(propValue) , "string") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_STRING );
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				pInfo->AddAttr ( tCol , true);
			}else
			if(strcmp(PyString_AsString(propValue) , "json") == 0){
				CSphColumnInfo tCol ( strkey, SPH_ATTR_JSON );
				tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
				pInfo->AddAttr ( tCol , true);
			}else
			//if(propValue == csft_string_fulltext)
			{
				//-> to support sql_field_string "string_text"
				if(strcmp(PyString_AsString(propValue) , "string_text") == 0){
					//append strings
					CSphColumnInfo tCol ( strkey, SPH_ATTR_STRING );
					tCol.m_iIndex = i; //m_tSchema.GetAttrsCount (); //should be i in pList?
					pInfo->AddAttr ( tCol , true);
				}else
				if(strcmp(PyString_AsString(propValue) , "text") != 0){
					sError.SetSprintf("Type %s is invalid, treated as full-text", PyString_AsString(propValue));
				}
				//default fulltext field
				// AddFieldToSchema(skey.cstr(), i);
				{
					CSphColumnInfo tCol ( strkey );
					// TODO: setup prefix, infix
					//this used to set SPH_WORDPART_PREFIX | SPH_WORDPART_INFIX, in push hit mode, no needs at all.
					SetupFieldMatch ( tCol ); 
					tCol.m_iIndex = i; 
					pInfo->m_dFields.Add ( tCol );
				}
			}				
		}else{
			///XXX? report error | continue;
		}
	} // for

	if(!doc_id_col) {
		if(PyErr_Occurred()) PyErr_Print();
		sError.SetSprintf("Must set docid = True attribute in DataSource Scheme to declare document unique id");
		PyErr_Clear();
		return -1;
	}
	docid = doc_id_col;

	return 0;
}
//////////////////////////////////////////////////////////////////////////

int CSphSource_Python::InitDataSchema_Python( CSphString & sError )
{
	if (!m_pInstance) {
		PyErr_Print();
		m_sError.SetSprintf ( "Can no create source object");
		return -1;
	}
	//all condition meets
	m_tSchema.m_dFields.Reset ();
	
	
	//enum all attrs
	PyObject* pArgs = NULL;
    PyObject* pResult = NULL; 
    PyObject* pFunc = PyObject_GetAttrString(m_pInstance, "GetSchema"); // +1
	if (!pFunc) {
		sError = ("Method SourceObj->GetSchema():{dict of attributes} missing.");
		//fprintf(stderr,m_sError.cstr());
		return -2; //Next Document must exist
	}

    if(!pFunc||!PyCallable_Check(pFunc)){
        Py_XDECREF(pFunc);
		sError = ("Method SourceObj->GetSchema():{dict of attributes} missing.");
		//fprintf(stderr,m_sError.cstr());
        return -2;
    }
    pArgs  = Py_BuildValue("()");

    pResult = PyEval_CallObject(pFunc, pArgs);    
    Py_XDECREF(pArgs);
    Py_XDECREF(pFunc);

	if(PyErr_Occurred()) PyErr_Print();
	
	{
		int nRet = UpdatePySchema(pResult, &m_tSchema, m_Doc_id_col, sError);
		if(nRet)
			return nRet;
	}

    Py_XDECREF(pResult);

	/*
	char sBuf [ 1024 ];
	snprintf ( sBuf, sizeof(sBuf), "pysource(%s)", m_sCommand.cstr() );
	m_tSchema.m_sName = sBuf;
	*/

	m_tDocInfo.Reset ( m_tSchema.GetRowSize() );
	m_dStrAttrs.Resize ( m_tSchema.GetAttrsCount() );

	// check it
	if ( m_tSchema.m_dFields.GetLength()>SPH_MAX_FIELDS )
		LOC_ERROR2 ( "too many fields (fields=%d, max=%d); raise SPH_MAX_FIELDS in sphinx.h and rebuild",
			m_tSchema.m_dFields.GetLength(), SPH_MAX_FIELDS );
	
	return 0;
}

/// connect to the source (eg. to the database)
/// connection settings are specific for each source type and as such
/// are implemented in specific descendants
bool	CSphSource_Python::Connect ( CSphString & sError ){
	//init the schema
	if (!m_pInstance) {
		PyErr_Print();
		sError.SetSprintf ( "Can no create source object");
		return false;
	}

	m_tHits.m_dData.Reserve ( m_iMaxHits );
	//try to do connect
	{
		if (!m_pInstance)
		{
			if(PyErr_Occurred()) PyErr_Print();
			PyErr_Clear();
			return false;
		}else{
			PyObject* pArgs = NULL;
			PyObject* pResult = NULL; 
			PyObject* pFunc = PyObject_GetAttrString(m_pInstance, "Connected");
			if (!pFunc){
				sError = ("'SourceObj->Connected():None' missing.\n");
				PyErr_Clear(); 
				//return false;
			}else{				 
				if(!PyCallable_Check(pFunc)){
					Py_XDECREF(pFunc);
					return false;
				}
				pArgs  = Py_BuildValue("()");

				pResult = PyEval_CallObject(pFunc, pArgs);    
				Py_XDECREF(pArgs);
				Py_XDECREF(pFunc);
				//check result
				
				if(PyErr_Occurred()) PyErr_Print();

				if(!CheckResult(pResult)) {
					Py_XDECREF(pResult);
					return false;
				} 
				Py_XDECREF(pResult);
			}
			
			// init data-schema when connect
			if(InitDataSchema_Python(sError) != 0)
				return false;

		}
	}
	return true;
}
bool CSphSource_Python::CheckResult(PyObject * pResult)
{
	if(!pResult) //no return default true
		return true;

	if(PyBool_Check(pResult)){
		if(Py_False == pResult)			
			return false;
	}
	if(PyInt_Check(pResult)) {
		if(PyInt_AsLong(pResult) == 0)
			return false;
	}
	return true;
}
/// disconnect from the source
void	CSphSource_Python::Disconnect (){
	m_tSchema.Reset ();
	m_tHits.m_dData.Reset();
}
/// check if there are any attributes configured
/// note that there might be NO actual attributes in the case if configured
/// ones do not match those actually returned by the source
bool	CSphSource_Python::HasAttrsConfigured () {
	return true;
}

void	CSphSource_Python::PostIndex ()
{
	if (!m_pInstance)
	{
		PyErr_Print();
		goto DONE;
	}else{
		PyObject* pArgs = NULL;
		PyObject* pResult = NULL; 
		PyObject* pFunc = PyObject_GetAttrString(m_pInstance, "OnIndexFinished");
		if (!pFunc)
			PyErr_Clear(); //is function can be undefined

		if(!pFunc || pFunc == Py_None ||!PyCallable_Check(pFunc)){
			Py_XDECREF(pFunc);
			goto DONE;
		}
		pArgs  = Py_BuildValue("()");

		pResult = PyEval_CallObject(pFunc, pArgs);
		
		if(PyErr_Occurred()) PyErr_Print();
		PyErr_Clear();

		Py_XDECREF(pArgs);
		Py_XDECREF(pFunc);
		Py_XDECREF(pResult);
	}
DONE:
	return ;
}

/// begin iterating document hits
/// to be implemented by descendants
bool	CSphSource_Python::IterateStart ( CSphString & sError ){
	int iFieldMVA = 0;	
	///TODO: call on_before_index function
	if (!m_pInstance)
	{
		PyErr_Print();
		goto DONE;
	}else{
		PyObject* pArgs = NULL;
		PyObject* pResult = NULL; 
		PyObject* pFunc = PyObject_GetAttrString(m_pInstance, "OnBeforeIndex");
		if (!pFunc)
			PyErr_Clear(); //is function can be undefined

		if(!pFunc  || pFunc == Py_None ||!PyCallable_Check(pFunc)){
			Py_XDECREF(pFunc);
			goto DONE;
		}
		pArgs  = Py_BuildValue("()");

		pResult = PyEval_CallObject(pFunc, pArgs);    
		Py_XDECREF(pArgs);
		Py_XDECREF(pFunc);

		if(PyErr_Occurred()) PyErr_Print();
		PyErr_Clear();

		if(!CheckResult(pResult)) {
			Py_XDECREF(pResult);
			return false;
		} 
		Py_XDECREF(pResult);

		
	}

DONE:

	//process GetFieldOrder
	{
		PyObject* pArgs = NULL;
		PyObject* pResult = NULL; 
		PyObject* pFunc = PyObject_GetAttrString(m_pInstance, "GetFieldOrder");
		if (!pFunc)
			PyErr_Clear(); //is function can be undefined

		if(!pFunc||!PyCallable_Check(pFunc)){
			Py_XDECREF(pFunc);
			//goto DONE;
		}
		pArgs  = Py_BuildValue("()");

		pResult = PyEval_CallObject(pFunc, pArgs);    
		Py_XDECREF(pArgs);
		Py_XDECREF(pFunc);

		if(PyErr_Occurred()) PyErr_Print();
		PyErr_Clear();

		//this is list -> list is supported by python source level-3
		if (PyList_Check(pResult)) {
			int iSize = (int)PyList_Size(pResult);
			if(iSize>=1) { //must have at least one element
				PyObject* pResult2 = PyList_GetItem(pResult,0); //borrow
				m_iPlainFieldsLength = 0;

				if (PyTuple_Check(pResult2)) {

					m_iPlainFieldsLength = (int)PyTuple_Size(pResult2); //the base fields.
#if HAVE_SSIZE_T
					for(Py_ssize_t  iField = 0; iField< PyTuple_Size(pResult2); iField++)
#else
					for(int iField = 0; iField< PyTuple_Size(pResult2); iField++)
#endif
					{
						PyObject* pItem = PyTuple_GetItem(pResult2, iField);
						if(PyString_Check(pItem)){
							//int j = this->m_tSchema.GetFieldIndex (PyString_AsString(pItem));
							this->m_baseFields.Add(PyString_AsString(pItem));
						}
					}
				}//end PyTuple_Check
				else
				//might be string only
				if(PyString_Check(pResult2)){
					this->m_baseFields.Add(PyString_AsString(pResult2));
				}
				else{
					sError.SetSprintf ( "Must have at least one field as a first order field" );
					return 0;
				}
			}//end if iSize >= 1

			if(iSize==2) { //have joined fields
				PyObject* pResult2 = PyList_GetItem(pResult,1); //borrow

				if (PyTuple_Check(pResult2)) {
#if HAVE_SSIZE_T
					for(Py_ssize_t  iField = 0; iField< PyTuple_Size(pResult2); iField++)
#else
					for(int iField = 0; iField< PyTuple_Size(pResult2); iField++)
#endif
					{
						PyObject* pItem = PyTuple_GetItem(pResult2, iField);
						if(PyString_Check(pItem)){
							int j = this->m_tSchema.GetFieldIndex (PyString_AsString(pItem));
							this->m_joinFields.Add(PyString_AsString(pItem));
							if(j!=-1) {
								// update mindex
								m_tSchema.m_dFields[j].m_iIndex = -1;
							}
						} // end string check
					}
				}//end PyTuple_Check
				if(PyString_Check(pResult2)){
					this->m_joinFields.Add(PyString_AsString(pResult2));
				}
			}
		}else
		{
			sError = ("Method SourceObj->GetFieldOrder():[list of fields tuple]). Only list is acceptable.");
			Py_XDECREF(pResult);
			return false;
		}
		Py_XDECREF(pResult);
	} //end of get field order

	// process of prefetch GetDocField, DocField can be NULL, if no delay collection field.
	{
		if(m_pInstance_GetDocField == NULL)
		{
			//PyObject* pFunc = NULL;
			if (m_pInstance)
			{
				m_pInstance_GetDocField = PyObject_GetAttrString(m_pInstance, "GetDocField");
			}

			if(!m_pInstance_GetDocField){
				// GetDocField CAN '404 Not found.'
				if(PyErr_Occurred())
					PyErr_Clear();
				// just pass it.
			}
		}
	}
	// process MVA checking.
	m_iFieldMVA = 0;
	m_iFieldMVAIterator = 0;
	m_dAttrToFieldMVA.Resize ( 0 );

	for ( int i = 0; i < m_tSchema.GetAttrsCount (); i++ )
	{
		const CSphColumnInfo & tCol = m_tSchema.GetAttr ( i );
		if ( ( tCol.m_eAttrType == SPH_ATTR_UINT32SET || tCol.m_eAttrType == SPH_ATTR_INT64SET ) && tCol.m_eSrc == SPH_ATTRSRC_FIELD )
			m_dAttrToFieldMVA.Add ( iFieldMVA++ );
		else
			m_dAttrToFieldMVA.Add ( -1 );
	}

	m_dFieldMVAs.Resize ( iFieldMVA );
	ARRAY_FOREACH ( i, m_dFieldMVAs )
		m_dFieldMVAs [i].Reserve ( 16 );

	return true;
}

/// begin iterating values of out-of-document multi-valued attribute iAttr
/// will fail if iAttr is out of range, or is not multi-valued
/// can also fail if configured settings are invalid (eg. SQL query can not be executed)
bool	CSphSource_Python::IterateMultivaluedStart ( int iAttr, CSphString & sError )
{
	if ( iAttr<0 || iAttr>=m_tSchema.GetAttrsCount() )
		return false;

	m_iMultiAttr = iAttr;
	const CSphColumnInfo & tAttr = m_tSchema.GetAttr(iAttr);

	if ( !(tAttr.m_eAttrType==SPH_ATTR_UINT32SET || tAttr.m_eAttrType==SPH_ATTR_INT64SET ) )
		return false;

	switch ( tAttr.m_eSrc )
	{
	case SPH_ATTRSRC_FIELD:
		return false;
	case SPH_ATTRSRC_QUERY:
		{
			if(m_pInstance_GetMVAValue == NULL)
			{
				//PyObject* pFunc = NULL;
				if (m_pInstance)
				{
					m_pInstance_GetMVAValue = PyObject_GetAttrString(m_pInstance, "GetMVAValue");
				}

				if(!m_pInstance_GetMVAValue){
					// GetDocField CAN '404 Not found.'
					if(PyErr_Occurred())
						PyErr_Clear();
					sError = ("Method SourceObj->GetMVAValue():(docid, [list of values]) missing.");
					return false;
				}
			}
			if(m_pInstance_GetMVAValue)
				return true;
		}
	default:
		sError.SetSprintf ( "INTERNAL ERROR: unknown multi-valued attr source type %d", tAttr.m_eSrc );
		return false;
	} // end of switch
	//return true;
}

/// get next multi-valued (id,attr-value) tuple to m_tDocInfo
bool	CSphSource_Python::IterateMultivaluedNext ()
{	
	const CSphColumnInfo & tAttr = m_tSchema.GetAttr ( m_iMultiAttr );
	assert ( tAttr.m_eAttrType==SPH_ATTR_UINT32SET || tAttr.m_eAttrType==SPH_ATTR_INT64SET );
	
	//m_iMultiAttr
	if(m_pInstance_GetMVAValue)
	{
		//call GetMVA
		{
			PyObject* pArgs = NULL;
			PyObject* pResult = NULL; 

			//create hit_c
			pArgs  = Py_BuildValue("(sO)", tAttr.m_sName.cstr(), Py_None);
			pResult = PyEval_CallObject(m_pInstance_GetMVAValue, pArgs);    

			if(PyErr_Occurred()) PyErr_Print();
			PyErr_Clear();

			Py_XDECREF(pArgs);

			if(Py_None == pResult || pResult == NULL) {
				return false;
			}

			if(!PyTuple_Check(pResult)){
				Py_XDECREF(pResult);
				//sError.SetSprintf("SourceObj->GetMVAValue():(id, [list]) missing, Can not continue");
				return false;
			}
			if(PyTuple_GET_SIZE(pResult) == 2) {

				//const CSphAttrLocator & tLoc = m_tSchema.GetAttr ( m_iMultiAttr ).m_tLocator;
				// (docid, text)
				PyObject* pDocId = PyTuple_GetItem(pResult, 0);
				PyObject* py_var = PyTuple_GetItem(pResult, 1);

				// convert docid
				if(pDocId && PyLong_Check(pDocId)) {
#if USE_64BIT
					m_tDocInfo.m_uDocID  =  PyLong_AsUnsignedLongLong(pDocId);
#else
					m_tDocInfo.m_uDocID = (SphDocID_t)(PyLong_AsLong(pDocId));
#endif
				}else
					if(pDocId && PyInt_Check(pDocId))
						m_tDocInfo.m_uDocID =  PyInt_AsLong(pDocId);
				/*
					// return that tuple or offset to storage for MVA64 value
					m_tDocInfo.m_iDocID = sphToDocid ( SqlColumn(0) );
					m_dMva.Resize ( 0 );
					if ( tAttr.m_eAttrType==SPH_ATTR_UINT32SET )
						m_dMva.Add ( sphToDword ( SqlColumn(1) ) );
					else
						sphAddMva64 ( m_dMva, sphToUint64 ( SqlColumn(1) ) );
				*/
				
				m_dMva.Resize ( 0 );
				int64_t d64Val = 0;
				DWORD dVal  = 0;
				if(PyInt_Check(py_var)) 
				{
					dVal = (DWORD)(PyInt_AsUnsignedLongMask(py_var));
					d64Val = dVal;
				}
				if(PyLong_Check(py_var)) 
				{
					d64Val = (int64_t)(PyLong_AsLongLong(py_var));
					dVal = (DWORD)d64Val; //might overflow, but never mind.
				}

				//do real assign
				if ( tAttr.m_eAttrType==SPH_ATTR_UINT32SET )
					m_dMva.Add ( dVal );
				else
					sphAddMva64 ( m_dMva, d64Val );
			}
			Py_XDECREF(pResult);
		}
	}
	// return that tuple
	return true;
}

/// begin iterating values of multi-valued attribute iAttr stored in a field
/// will fail if iAttr is out of range, or is not multi-valued
bool	CSphSource_Python::IterateFieldMVAStart ( int iAttr, CSphString & /* sError */ ){
	
	if ( iAttr<0 || iAttr>=m_tSchema.GetAttrsCount() )
		return false;

	if ( m_dAttrToFieldMVA [iAttr] == -1 )
		return false;

	//if ( !(tAttr.m_eAttrType==SPH_ATTR_UINT32SET || tAttr.m_eAttrType==SPH_ATTR_UINT64SET ) )
	//	return false;

	m_iFieldMVA = iAttr;
	m_iFieldMVAIterator = 0;
	return true;
}



int CSphSource_Python::ParseFieldMVA ( CSphVector < DWORD > & dMva, PyObject * pList, bool bMva64) {

	if(!pList)
		return 0;
	if(!PyList_Check(pList)) 
		return 0;

	assert ( dMva.GetLength() ); // must not have zero offset
	int uOff = dMva.GetLength();
	dMva.Add ( 0 ); // reserve value for count
	
	/*
	if ( pDigit )
	{
		if ( !bMva64 )
			dMva.Add ( sphToDword ( pDigit ) );
		else
			sphAddMva64 ( dMva, sphToUint64 ( pDigit ) );
	}
	*/
	size_t size = PyList_Size(pList);
	for(size_t j = 0; j < size; j++) {
		//PyList_GetItem just a borrowed reference
		PyObject* py_var = PyList_GetItem(pList,j);
		
		uint64_t d64Val = 0;
		DWORD dVal  = 0;
		if(PyInt_Check(py_var)) 
		{
			dVal = (DWORD)(PyInt_AsUnsignedLongMask(py_var));
			d64Val = dVal;
		}
		if(PyLong_Check(py_var)) 
		{
			d64Val = (uint64_t)(PyInt_AsUnsignedLongLongMask(py_var));
			dVal = (DWORD)d64Val; //might overflow, but never mind.
		}
		// FIXME: here is a bug in MVA64
		if ( !bMva64 )
			m_dMva.Add ( dVal );
		else
			sphAddMva64 ( m_dMva, d64Val );
	}

	int iCount = dMva.GetLength()-uOff-1;
	if ( !iCount )
	{
		dMva.Pop(); // remove reserved value for count in case of 0 MVAs
		return 0;
	} else
	{
		dMva[uOff] = iCount;
		return uOff; // return offset to ( count, [value] )
	}
	return 0;
}

/// get next multi-valued (id,attr-value) tuple to m_tDocInfo -> seems abandoned
bool	CSphSource_Python::IterateFieldMVANext (){
	int iFieldMVA = m_dAttrToFieldMVA [m_iFieldMVA];
	if ( m_iFieldMVAIterator >= m_dFieldMVAs [iFieldMVA].GetLength () )
		return false;

	const CSphColumnInfo & tAttr = m_tSchema.GetAttr ( m_iFieldMVA );
	//m_tDocInfo.SetAttr ( tAttr.m_tLocator, m_dFieldMVAs [iFieldMVA][m_iFieldMVAIterator] );
	m_dMva.Resize ( 0 );
	
	{
		uint64_t d64Val = 0;
		DWORD dVal  = 0;
		dVal = d64Val = m_dFieldMVAs [iFieldMVA][m_iFieldMVAIterator];
		if ( tAttr.m_eAttrType==SPH_ATTR_UINT32SET )
			m_dMva.Add ( dVal );
		else
			sphAddMva64 ( m_dMva, d64Val );
	}
	++m_iFieldMVAIterator;
	return true;
}

BYTE* CSphSource_Python::GetField ( int iFieldIndex)
{
	assert(iFieldIndex < m_tSchema.m_dFields.GetLength());

	//int iEndField = m_tSchema.m_iBaseFields;

	//check cache
	if(m_dFields[iFieldIndex])
		return m_dFields[iFieldIndex];

	//pysource is case sensitive
#if USE_PYTHON_CASE_SENSIVE_ATTR
	char* ptr_Name = (char*)m_tSchema.m_dFields[iFieldIndex].m_sNameExactly.cstr();
#else
	char* ptr_Name = (char*)m_tSchema.m_dFields[iFieldIndex].m_sName.cstr();
#endif

	PyObject* item = PyObject_GetAttrString(m_pInstance,ptr_Name);

	if(PyErr_Occurred()) PyErr_Print();
	PyErr_Clear();

	//PyList_GetItem(pList,m_tSchema.m_dFields[i].m_iIndex);
	//check as string?
	BYTE* ptr = NULL;
	if(item && Py_None!=item && PyString_Check(item)) {
		char* data = PyString_AsString(item);
		//m_dFields[i] = (BYTE*)PyString_AsString(item); //error!!! this pointer might be move later.
		ptr = (BYTE*)strdup(data);
	}
	//check is unicode?
	if(ptr == NULL && item && Py_None!=item && PyUnicode_Check(item)) {
		PyObject* utf8str = PyUnicode_EncodeUTF8(PyUnicode_AS_UNICODE(item),
                                         PyUnicode_GET_SIZE(item),
                                         "ignore"); //ignore all error unicode char.
		if(utf8str) {
			//if convert successfully.
			char* data = PyString_AsString(utf8str);
			ptr = (BYTE*)strdup(data);
			Py_XDECREF(utf8str);
		}
	}
	//check is integer & long
	if(ptr == NULL && item && Py_None!=item && PyLong_Check(item)) {
		// convert long & integer to string.
		char buf[128];
		memset(buf,0,sizeof(buf));
		PY_LONG_LONG dVal = PyLong_AsLongLong(item);
#ifdef _WIN32
		snprintf(buf,sizeof(buf), "%I64u", dVal);
#else
		snprintf(buf,sizeof(buf), "%llu", dVal);
#endif
		ptr = (BYTE*)strdup(buf);
	}
	
	if(ptr == NULL && item && Py_None!=item && PyInt_Check(item)) {
		// convert long & integer to string.
		long dVal = PyInt_AsLong(item);
		char buf[128];
		memset(buf,0,sizeof(buf));
		snprintf(buf,sizeof(buf), "%ld", dVal);
		ptr = (BYTE*)strdup(buf);
	}

	Py_XDECREF(item);
	m_dFields[iFieldIndex] = ptr;
	return ptr;
}

BYTE **	CSphSource_Python::NextDocument ( CSphString & sError ){
	//1st call document to load the data into pyobject.
	//call on_nextdocument function in py side.
	//call get_docId function to get the DocID attr's name
	// __getattr__ not work with PyObject_GetAttrString
	//clean the m_dFields 's data
	unsigned int iPrevHitPos = 0;

	ARRAY_FOREACH ( i, m_tSchema.m_dFields ) {
		if(m_dFields[i])
			free(m_dFields[i]);
		m_dFields[i] = NULL;
	}

	m_tDocInfo.Reset ( m_tSchema.GetRowSize() );

	if(m_Doc_id_col.IsEmpty()) 
		return NULL; //no init yet!

	if(m_pInstance_NextDocument == NULL)
	{
		//PyObject* pFunc = NULL;
		if (m_pInstance)
		{
			m_pInstance_NextDocument = PyObject_GetAttrString(m_pInstance, "NextDocument");
		}

		if(!m_pInstance_NextDocument){
			// BuildHits CAN '404 Not found.'
			if(PyErr_Occurred())
				PyErr_Clear();

			sError.SetSprintf("SourceObj->NextDocument():Bool missing, Can not continue");
			return NULL; //Next Document must exist
		}
	}

	PyObject* pDocInfo = m_pInstance;

	if(!m_pInstance_NextDocument)
		return NULL;

	{
		//call next_document
		if (!m_pInstance)
		{
			PyErr_Print();
			return NULL;
		}else{
			PyObject* pArgs = NULL;
			PyObject* pResult = NULL; 
			PyObject* pFunc = m_pInstance_NextDocument;
	
			//mark the hit position
			m_tHits.m_dData.Resize( 0 );
			iPrevHitPos = m_tHits.m_dData.GetLength(); 

			pArgs  = Py_BuildValue("(O)", Py_None);

			pResult = PyEval_CallObject(pFunc, pArgs);    
			Py_XDECREF(pArgs);

			if(PyErr_Occurred()) PyErr_Print();
			PyErr_Clear();

			if(!pResult) {
				sError.SetSprintf("Exception happens in python source");
				m_tDocInfo.m_uDocID = 0;
				goto CHECK_TO_CALL_AFTER_INDEX;
			}

			if(PyBool_Check(pResult) && pResult == Py_False){
				Py_XDECREF(pResult);
				m_tDocInfo.m_uDocID = 0;
				goto CHECK_TO_CALL_AFTER_INDEX;
				//return NULL; //if return false , the source finished
			} 
			// check result is True | document object
			if(PyBool_Check(pResult) && pResult == Py_True){
				Py_XDECREF(pResult);
			} else {
				pDocInfo = pResult;
			}
			//We do NOT care about doc_id, but doc_id must be > 0
		}
	}
	{
		PyObject* pDocId = GetObjectAttr(pDocInfo, (char*)m_Doc_id_col.cstr());

		if(pDocId && PyLong_Check(pDocId)) {
#if USE_64BIT
			m_tDocInfo.m_uDocID  =  PyLong_AsUnsignedLongLong(pDocId);
#else
			m_tDocInfo.m_uDocID = (SphDocID_t)(PyLong_AsLong(pDocId));
#endif
		}else
			if(pDocId && PyInt_Check(pDocId))
				m_tDocInfo.m_uDocID =  PyInt_AsLong(pDocId);

		Py_XDECREF(pDocId);

		m_uMaxFetchedID = Max ( m_uMaxFetchedID, m_tDocInfo.m_uDocID );
		
		//re-set docid, user might push hit while in nextdocument
		for(; iPrevHitPos < m_tHits.m_dData.GetLength(); iPrevHitPos++) {
			m_tHits.m_dData[iPrevHitPos].m_uDocID = m_tDocInfo.m_uDocID;
		}

	}
CHECK_TO_CALL_AFTER_INDEX:
	//check doc_id
	if(m_tDocInfo.m_uDocID == 0)
	{
		//call sql_query_post
		PyObject* pArgs = NULL;
		PyObject* pResult = NULL; 
		PyObject* pFunc = PyObject_GetAttrString(m_pInstance, "OnAfterIndex");
		if (!pFunc)
			PyErr_Clear(); //is function can be undefined

		if(!pFunc || pFunc == Py_None ||!PyCallable_Check(pFunc)){
			Py_XDECREF(pFunc);
			goto DONE;
		}
		pArgs  = Py_BuildValue("()");

		pResult = PyEval_CallObject(pFunc, pArgs);    

		if(PyErr_Occurred()) PyErr_Print();
		PyErr_Clear();

		Py_XDECREF(pArgs);
		Py_XDECREF(pFunc);
		Py_XDECREF(pResult); //we do not care about the result.
DONE:
		return NULL;
	}

	int iFieldMVA = 0;
	for ( int i=0; i<m_tSchema.GetAttrsCount(); i++ ) {
		const CSphColumnInfo & tAttr = m_tSchema.GetAttr(i); // shortcut
		if ( tAttr.m_eAttrType == SPH_ATTR_UINT32SET || tAttr.m_eAttrType == SPH_ATTR_INT64SET)
		{
			m_tDocInfo.SetAttr ( tAttr.m_tLocator,0);
			if ( tAttr.m_eSrc == SPH_ATTRSRC_FIELD ) {
				//all the MVA fields in this data source is SPH_ATTRSRC_FIELD
				//deal the python-list
#if USE_PYTHON_CASE_SENSIVE_ATTR
				PyObject* pList = PyObject_GetAttrString(pDocInfo, (char*)tAttr.m_sNameExactly.cstr());
#else
				PyObject* pList = PyObject_GetAttrString(pDocInfo, (char*)tAttr.m_sName.cstr());
#endif
				if(PyErr_Occurred()) PyErr_Print();
				PyErr_Clear();
				if(!pList)
					return NULL;
				if(PyList_Check(pList)) {
					/*
					size_t size = PyList_Size(pList);
					m_dFieldMVAs [iFieldMVA].Resize ( 0 );
					for(size_t j = 0; j < size; j++) {
						//PyList_GetItem just a borrowed reference
						PyObject* item = PyList_GetItem(pList,j);
						PY_LONG_LONG dVal =  0;
						if(item && (PyInt_Check(item)))
							dVal = PyInt_AsLong(item);
						if(item && (PyLong_Check(item)))
							dVal = PyLong_AsLongLong(item);
				
						m_dFieldMVAs [iFieldMVA].Add ( (DWORD)dVal);
					}
					*/
					int uOff = 0;
					if ( tAttr.m_eSrc==SPH_ATTRSRC_FIELD )
					{
						uOff = ParseFieldMVA ( m_dMva, pList, tAttr.m_eAttrType==SPH_ATTR_INT64SET );
					}
					m_tDocInfo.SetAttr ( tAttr.m_tLocator, uOff );
					//continue;
				}else{
					if(pList!= Py_None) {
						sError.SetSprintf("List expected for attribute, @%s." , (char*)tAttr.m_sName.cstr());
						return NULL;
					}
				}
				/// <- XXX: hacking, should take care of const reference
				CSphColumnInfo & tAttr2 = const_cast<CSphColumnInfo&>(tAttr);
				tAttr2.m_iMVAIndex = iFieldMVA; //assign the index.

				iFieldMVA++;
				Py_XDECREF(pList);
			}
			continue;
		}
		//deal other attributes
#if USE_PYTHON_CASE_SENSIVE_ATTR
		PyObject* item = PyObject_GetAttrString(pDocInfo, (char*)tAttr.m_sNameExactly.cstr()); //+1
#else
		PyObject* item = PyObject_GetAttrString(pDocInfo, (char*)tAttr.m_sName.cstr()); //+1
#endif
		if(PyErr_Occurred()) PyErr_Print();
		PyErr_Clear();
		
		SetAttr(i, item);
		//Py_XDECREF(item);
	}
	ARRAY_FOREACH ( i, m_tSchema.m_dFields ) {
		// set m_dFields
                if(m_tSchema.m_dFields[i].m_iIndex < 0 )
                    continue; // -1 the join field.
		char* ptr_Name = (char*)m_tSchema.m_dFields[i].m_sName.cstr();
		PyObject* item = PyObject_GetAttrString(pDocInfo,ptr_Name);

		if(PyErr_Occurred()) {
			PyErr_Print();
			PyErr_Clear();
			continue;
		}

		//check as string?
		BYTE* ptr = NULL;
		if(item && Py_None!=item && PyString_Check(item)) {
			char* data = PyString_AsString(item);
			//m_dFields[i] = (BYTE*)PyString_AsString(item); //error!!! this pointer might be move later.
			ptr = (BYTE*)strdup(data);
		}
		//check is unicode?
		if(ptr == NULL && item && Py_None!=item && PyUnicode_Check(item)) {
			PyObject* utf8str = PyUnicode_EncodeUTF8(PyUnicode_AS_UNICODE(item),
	                                         PyUnicode_GET_SIZE(item),
	                                         "ignore"); //ignore all error unicode char.
			if(utf8str) {
				//if convert successfully.
				char* data = PyString_AsString(utf8str);
				ptr = (BYTE*)strdup(data);
				Py_XDECREF(utf8str);
			}
		}
		//check is integer & long
		if(ptr == NULL && item && Py_None!=item && PyLong_Check(item)) {
			// convert long & integer to string.
			char buf[128];
			memset(buf,0,sizeof(buf));
			PY_LONG_LONG dVal = PyLong_AsLongLong(item);
	#ifdef _WIN32
			snprintf(buf,sizeof(buf), "%I64u", dVal);
	#else
			snprintf(buf,sizeof(buf), "%llu", dVal);
	#endif
			ptr = (BYTE*)strdup(buf);
		}
		
		if(ptr == NULL && item && Py_None!=item && PyInt_Check(item)) {
			// convert long & integer to string.
			long dVal = PyInt_AsLong(item);
			char buf[128];
			memset(buf,0,sizeof(buf));
			snprintf(buf,sizeof(buf), "%ld", dVal);
			ptr = (BYTE*)strdup(buf);
		}

		Py_XDECREF(item);
		//printf("set field %s @ %d \n", ptr_Name, i);
		m_dFields[i] = ptr;
	} // end for each fields.
	
	// release
	if(pDocInfo != m_pInstance)
		Py_XDECREF(pDocInfo);

	return m_dFields;
}

ISphHits * CSphSource_Python::IterateJoinedHits ( CSphString & sError){
	// - join field.
	m_tHits.m_dData.Resize ( 0 );

	// eof check
	if ( m_iJoinedHitField >= m_joinFields.GetLength() )
	{
		m_tDocInfo.m_uDocID = 0;
		return &m_tHits;
	}

	/*
		NOTE; A bit complicated still, for a joint field might be very large. Needs some mechanism to break the loop.
			  Or, total memory might be run out.
	*/
	// check GetDocField's existence
	if(m_pInstance_GetDocField == NULL) 
	{
		sError.SetSprintf("SourceObj->GetDocField():(id, string) missing, Can not continue");
		return NULL;
	}
	
	/*
	- m_iJoinedHitPos: mark current field's indexing pos. ->should be an array.,
	- m_iJoinedHitField
	*/

	if(m_joinFields.GetLength() !=0)
	{
		while(m_iJoinedHitField < m_joinFields.GetLength())
		{
			//the position in schema field.
			int iJoinedHitField = m_tSchema.GetFieldIndex (m_joinFields[m_iJoinedHitField].cstr());
			if(iJoinedHitField == -1) {
				fprintf(stderr, "Can Not found field named %s, skipping\n" , m_joinFields[m_iJoinedHitField].cstr());
				m_iJoinedHitField ++;
				continue;
			}

			// join: call GetDocField, -> return (docid, text)
			while (true)
			{
				if(m_tState.m_bProcessingHits)
				{
					// if still in processing, continue the job.
					CSphSource_Document::BuildHits ( sError, true );
					// update current position
					if ( !m_tSchema.m_dFields[m_iJoinedHitField].m_bPayload && !m_tState.m_bProcessingHits && m_tHits.Length() )
						m_iJoinedHitPositions[iJoinedHitField] = HITMAN::GetPos ( m_tHits.Last()->m_uWordPos );

					if(m_tState.m_bProcessingHits)
						break; //too large in one pass.

					// check and free ptr -> strdup created.
					if(m_dFields[0] && !m_tState.m_bProcessingHits) {
						free(m_dFields[0]);
						m_dFields[0] = NULL;
					}

				}

				PyObject* pArgs = NULL;
				PyObject* pResult = NULL; 
				PyObject* pResultItem = NULL; 
				PyObject* pResultKeys = NULL;
#if HAVE_SSIZE_T
				Py_ssize_t key_count = 0;
				Py_ssize_t i = 0;
#else
				int key_count = 0;
				int i = 0;
#endif

				//create hit_c

				pArgs  = Py_BuildValue("(sO)", m_joinFields[m_iJoinedHitField].cstr(), Py_None);
				
				//如果已经有值，则不继续处理

				if(!m_JoinFieldsResult) 					
					pResult = PyEval_CallObject(m_pInstance_GetDocField, pArgs);    
				else
					pResult = m_JoinFieldsResult;

				if(PyErr_Occurred()) PyErr_Print();
				PyErr_Clear();

				Py_XDECREF(pArgs);

				if(Py_None == pResult || pResult == NULL) {
					//no more data, in this field!(all document).
					m_iJoinedHitID = 0;
					m_iJoinedHitField ++ ;
					memset(m_iJoinedHitPositions,0,sizeof(m_iJoinedHitPositions));
					break; //this field is over, next.
				}
				
				/*
				* 返回值可能是 Dict 或者 Tuple
				*/
				if(!PyDict_Check(pResult)) 
					pResultItem = pResult; //is the tuple.
				else{
					pResultKeys = PyDict_Keys(pResult); //+1
					key_count = PyList_GET_SIZE(pResultKeys);
					if(pResult == m_JoinFieldsResult)
						i = m_JoinFieldsResultPos;
					else
						i = 0;
				}
				
				while(true) {
					
					if(pResultKeys && i>= key_count) break;  //stop the while loop

					if(pResultKeys) {
						PyObject* pItemKey = PyList_GET_ITEM(pResultKeys, i); //+0
						
						if (! PyString_Check(pItemKey) && !PyUnicode_Check(pItemKey)) {
							i++;
							continue; //if return a hash table, field name must be string
						}

						iJoinedHitField = -1;
						pResultItem = PyDict_GetItem(pResult, pItemKey);

						if(PyUnicode_Check(pItemKey)) {
							PyObject* utf8str = PyUnicode_EncodeUTF8(PyUnicode_AS_UNICODE(pItemKey),
									PyUnicode_GET_SIZE(pItemKey),
									"ignore"); //ignore all error unicode char. +1
								if(utf8str) {
									//if convert successfully.
									iJoinedHitField = m_tSchema.GetFieldIndex (PyString_AsString(utf8str));
								}
						}else{
							iJoinedHitField = m_tSchema.GetFieldIndex (PyString_AsString(pItemKey));
						}
						if(iJoinedHitField == -1) {
							//The Column PySource Given not found.
							fprintf(stderr, "[Error in PySource] Can Not found field named %s, skipping\n" ,PyString_AsString(pItemKey));
							i++; 
							continue;;
						}
					}

					// check the result.
					if(!PyTuple_Check(pResultItem)){
						sError.SetSprintf("SourceObj->GetDocField():(id, string) missing, Can not continue");
						return NULL;
					}

					if(PyTuple_GET_SIZE(pResultItem) >= 2) {
						// (docid, text)
						PyObject* pDocId = PyTuple_GetItem(pResultItem, 0);
						PyObject* py_text = PyTuple_GetItem(pResultItem, 1);

						// convert docid
						if(pDocId && PyLong_Check(pDocId)) {
#if USE_64BIT
							m_tDocInfo.m_uDocID  =  PyLong_AsUnsignedLongLong(pDocId);
#else
							m_tDocInfo.m_uDocID = (SphDocID_t)(PyLong_AsLong(pDocId));
#endif
						}else
							if(pDocId && PyInt_Check(pDocId))
								m_tDocInfo.m_uDocID =  PyInt_AsLong(pDocId);

						if ( !m_iJoinedHitID )
							m_iJoinedHitID = m_tDocInfo.m_uDocID;

						// docid asc requirement violated? report an error
						if ( m_iJoinedHitID>m_tDocInfo.m_uDocID )
						{
							sError.SetSprintf ( "joined field '%s': query MUST return document IDs in ASC order",
								m_joinFields[m_iJoinedHitField].cstr() );
							return NULL;
						}

						// next document? update tracker, reset position
						if ( m_iJoinedHitID<m_tDocInfo.m_uDocID )
						{
							m_iJoinedHitID = m_tDocInfo.m_uDocID;
							memset(m_iJoinedHitPositions,0,sizeof(m_iJoinedHitPositions));
							//for(int i = 0; i< SPH_MAX_FIELDS; i++){		m_iJoinedHitPositions[i] = 0; 	}
						}

						// fetch text field
						BYTE* ptr = NULL;
						{
							PyObject* item = py_text;

							if(item && Py_None!=item && PyString_Check(item)) {
								char* data = PyString_AsString(item);
								//m_dFields[i] = (BYTE*)PyString_AsString(item); //error!!! this pointer might be move later.
								ptr = (BYTE*)strdup(data);
							}
							//check is unicode?
							if(ptr == NULL && item && Py_None!=item && PyUnicode_Check(item)) {
								PyObject* utf8str = PyUnicode_EncodeUTF8(PyUnicode_AS_UNICODE(item),
									PyUnicode_GET_SIZE(item),
									"ignore"); //ignore all error unicode char. +1
								if(utf8str) {
									//if convert successfully.
									char* data = PyString_AsString(utf8str);
									ptr = (BYTE*)strdup(data);
									Py_XDECREF(utf8str);
								}
							}
						}

						if ( !m_tState.m_bProcessingHits )
						{
							m_tState = CSphBuildHitsState_t();
							m_tState.m_iField = m_iJoinedHitField;
							m_tState.m_iStartField = m_iJoinedHitField;
							m_tState.m_iEndField = m_iJoinedHitField+1;
							m_tState.m_iStartPos = m_iJoinedHitPositions[iJoinedHitField];
							m_tState.m_dFields = m_dFields;
						}

						if(ptr)
						{
							m_dFields[0] = ptr; //set to zero -> avoid read from pysource.
							// build those hits
							if(PyTuple_GET_SIZE(pResultItem) == 3) {
								PyObject* py_ipos = PyTuple_GetItem(pResultItem, 2);
#if USE_64BIT
								DWORD uPosition = PyLong_AsLong(py_ipos); //use long as the doc it.
#else
								DWORD uPosition = (DWORD)(PyInt_AsLong(py_ipos));
#endif
								m_tState.m_iStartPos = uPosition;
							}

							{
								m_tState.m_iField = iJoinedHitField;
								m_tState.m_iStartField = iJoinedHitField;
								m_tState.m_iEndField = iJoinedHitField+1;
							}

							CSphSource_Document::BuildHits ( sError, true );
							// update current position
							if ( !m_tSchema.m_dFields[m_iJoinedHitField].m_bPayload && !m_tState.m_bProcessingHits && m_tHits.Length() )
								m_iJoinedHitPositions[iJoinedHitField] = HITMAN::GetPos ( m_tHits.Last()->m_uWordPos );
						}//end if ptr
						
						// check and free ptr -> strdup created.
						if(ptr && !m_tState.m_bProcessingHits) {
							m_dFields[0] = NULL;
							free(ptr);
						}

					} //end process pResultItem

					if(pResultKeys == NULL) break;
					i ++; //Why I increase i, for the tokenizer have stored the current fields context.
					if ( m_tState.m_bProcessingHits )  break; //break if m_tState is full.
				} //end while

				Py_XDECREF(pResultKeys);				
				
				if ( m_tState.m_bProcessingHits ) //break 
				{
					m_JoinFieldsResult = pResult; //save the result
					m_JoinFieldsResultPos = i;
					break;
				}else
				{
					// clear the continue
					if(m_JoinFieldsResult) {
						if(pResult == m_JoinFieldsResult)
							pResult = NULL;
						Py_XDECREF(m_JoinFieldsResult);
						m_JoinFieldsResult = NULL;
					}
				}
				Py_XDECREF(pResult); //we do not care about the result.
			} //end while
			if ( m_tState.m_bProcessingHits ) //break 
				break;
		} //end for
		
		//reset all.
		m_iJoinedHitID = 0;
		//TODO? how about continuous build
		memset(m_iJoinedHitPositions,0,sizeof(m_iJoinedHitPositions));
	}		

	// no more fields
	//m_tDocInfo.m_iDocID = 0; // pretend that's an eof
	return &m_tHits;
}

//////////////////////////////////////////////////////////////////////////
PyObject* CSphSource_Python::GetAttr(char* key)
{
	int iIndex = m_tSchema.GetAttrIndex(key);
	if(iIndex < 0){
		iIndex = m_tSchema.GetFieldIndex(key);
		if(iIndex < 0)
			return NULL;
		PyObject* item = PyObject_GetAttrString(m_pInstance, key);
		return item; //new refer, might leak memory? almost NOT
	}
	return PyObject_GetAttrString(m_pInstance, key);
}

int CSphSource_Python::SetAttr(char* key, PyObject* v)
{
	int iIndex = m_tSchema.GetAttrIndex(key);
	if(iIndex >= 0) { 
		int nRet = SetAttr(iIndex, v);
		PyObject_SetAttrString(m_pInstance, key, v); //set to the py document for easy getter code.
		return nRet;
	}

	iIndex = m_tSchema.GetFieldIndex(key);
	if(iIndex < 0)
		return -1;
	//set field values, for set on the python object is what we needs later
	return PyObject_SetAttrString(m_pInstance, key, v); 
}

int CSphSource_Python::SetAttr( int iIndex, PyObject* v)
{
	const CSphColumnInfo & tAttr = m_tSchema.GetAttr(iIndex); // shortcut
	if ( tAttr.m_eAttrType == SPH_ATTR_UINT32SET ){
		PyObject* pList = v;
		int iFieldMVA = tAttr.m_iMVAIndex;
		size_t size = PyList_Size(pList);
		m_dFieldMVAs [iFieldMVA].Resize ( 0 );
		for(size_t j = 0; j < size; j++) {
			//PyList_GetItem just a borrowed reference
			PyObject* item = PyList_GetItem(pList,j);
			PY_LONG_LONG dVal =  0;
			if(item && (PyInt_Check(item)))
				dVal = PyInt_AsLong(item);

			if(item && (PyLong_Check(item)))
				dVal = PyLong_AsLongLong(item);

			m_dFieldMVAs [iFieldMVA].Add ( (DWORD)dVal);
		}
		Py_XDECREF(pList);
	}

	PyObject* item = v;
	//normal attribute
	switch(tAttr.m_eAttrType){
		case SPH_ATTR_FLOAT:   {
			double dVal = 0.0;
			if( item == Py_None) {
				m_tDocInfo.SetAttr ( tAttr.m_tLocator, dVal);
			}
			if(item && PyFloat_Check(item))
				dVal = PyFloat_AsDouble(item);
			m_tDocInfo.SetAttrFloat ( tAttr.m_tLocator, (float)dVal);
			Py_XDECREF(item);
							}
		break;

		case SPH_ATTR_INTEGER:
		case  SPH_ATTR_BIGINT:{
			PY_LONG_LONG dVal =  0;
           if( item == Py_None) {
				m_tDocInfo.SetAttr ( tAttr.m_tLocator, dVal);
			}
			if(item && PyInt_Check(item)) {
				dVal =  PyInt_AsLong(item);
				m_tDocInfo.SetAttr ( tAttr.m_tLocator,(DWORD)dVal);
			}

			if(item && PyLong_Check(item)) {
				dVal =  PyLong_AsLongLong(item);
				m_tDocInfo.SetAttr ( tAttr.m_tLocator,dVal);
			}
			Py_XDECREF(item);
							   }
		break;
		case SPH_ATTR_BOOL: {
			PY_LONG_LONG dVal =  0;
			if(item && PyBool_Check(item)) 
				dVal =  (item == Py_True)?1:0;

			if(item && PyInt_Check(item)) 
				dVal =  PyInt_AsLong(item);

			m_tDocInfo.SetAttr ( tAttr.m_tLocator,(DWORD)dVal);
			Py_XDECREF(item);
							}
		break;
		case SPH_ATTR_TIMESTAMP: {
			//time stamp can be float and long
			PY_LONG_LONG dVal = 0;
           if( item == Py_None) {
				m_tDocInfo.SetAttr ( tAttr.m_tLocator, dVal);
			}
			if(item && PyLong_Check(item))
				dVal = PyLong_AsLongLong(item);
			if(item && PyFloat_Check(item))
				dVal = (long)PyFloat_AsDouble(item);
			m_tDocInfo.SetAttr (tAttr.m_tLocator,dVal);
			Py_XDECREF(item);
								 }
		break;
		case SPH_ATTR_STRING:
		case SPH_ATTR_JSON:
		{
			//check as string?
			if(item && Py_None!=item && PyString_Check(item)) {
				char* data = PyString_AsString(item);
				//if(m_dStrAttrs[iIndex].IsEmpty())
				//	m_dStrAttrs[iIndex].; //clear prev setting.
				m_dStrAttrs[iIndex] = data; //strdup(data); //same no needs to dup
			}else
			if( item && Py_None!=item && PyUnicode_Check(item)) {
				PyObject* utf8str = PyUnicode_EncodeUTF8(PyUnicode_AS_UNICODE(item),
							 PyUnicode_GET_SIZE(item),
							 "ignore"); //ignore all error unicode char.
				if(utf8str) {
					//if convert successfully.
					char* data = PyString_AsString(utf8str);
					m_dStrAttrs[iIndex] = data; //CSphString will clone the data
					//ptr = (BYTE*)strdup(data);
					Py_XDECREF(utf8str);
				}
			}
			Py_XDECREF(item);
								 }
		break;
		default:
			return -1;
			break;
	}

	return 0;
}

void CSphSource_Python::AddHit ( SphDocID_t uDocid, SphWordID_t uWordid, Hitpos_t uPos )
{
	m_tHits.AddHit ( uDocid, uWordid , uPos );
}

//////////////////////////////////////////////////////////////////////////
// helper functions
void CSphSource_Python::SetupFieldMatch ( CSphColumnInfo & tCol )
{
	const bool bWordDict = m_pDict->GetSettings().m_bWordDict;
	
	tCol.m_eWordpart = GetWordpart ( tCol.m_sName.cstr(), bWordDict );
}

void CSphSource_Python::AddFieldToSchema ( const char * szName , int iIndex)
{
	CSphColumnInfo tCol ( szName );
	SetupFieldMatch ( tCol );
	tCol.m_iIndex = iIndex; 
	m_tSchema.m_dFields.Add ( tCol );
}

int CSphSource_Python::InitDataSchema(const CSphConfigSection & hSource,const char* dsName) {
	
	PyObject* pFunc = PyObject_GetAttrString(main_module, "__coreseek_find_pysource");
	PyObject* m_pTypeObj = NULL;
	if(pFunc && PyCallable_Check(pFunc)){
		PyObject* pArgsKey  = Py_BuildValue("(s)", dsName);
		m_pTypeObj = PyEval_CallObject(pFunc, pArgsKey);
		Py_XDECREF(pArgsKey);
	} // end if
	if (pFunc)
		Py_XDECREF(pFunc);

	if (m_pTypeObj == NULL || m_pTypeObj == Py_None) {
		Error("Can NOT found data source %s.\n", dsName);
		return 0;
	}

	if (!PyClass_Check(m_pTypeObj) && !PyType_Check(m_pTypeObj)) {
		Py_XDECREF(m_pTypeObj);
		Error("%s is NOT a Python class.\n", dsName);
		return -1; //not a valid type file
	}
	
	if(!m_pTypeObj||!PyCallable_Check(m_pTypeObj)){
		Py_XDECREF(m_pTypeObj);
		return  -2;
	}else{
		PyObject* pConf = PyDict_New(); // +1
		hSource.IterateStart ();
		while ( hSource.IterateNext() ){
			//Add ( hSource.IterateGet(), hSource.IterateGetKey() );
			const char* key = hSource.IterateGetKey().cstr();

			CSphVector<CSphString>	values;
			LOC_GETAS(values, key);
			if(values.GetLength() >1)
			{
				PyObject* pVals = PyList_New(0); // +1
				ARRAY_FOREACH ( i, values )
				{
					PyList_Append(pVals, PyString_FromString(values[i].cstr()));
				}
				PyDict_SetItem(pConf, PyString_FromString(key), pVals); //0 ok ; -1 error
				Py_XDECREF(pVals);
			}else{
				const char* val = hSource.IterateGet().cstr();
				//hSource.IterateGet();
				PyDict_SetItemString(pConf, key, PyString_FromString(val));
			}
		}

		PyObject* pargs  = Py_BuildValue("O", pConf); //+1
		PyObject* pArg = PyTuple_New(1); //+1
		PyTuple_SetItem(pArg, 0, pargs); //steal one reference

		m_pInstance  = PyEval_CallObject(m_pTypeObj, pArg);   
		if(!m_pInstance){
			PyErr_Print();
			Py_XDECREF(pArg);
			Py_XDECREF(m_pTypeObj);
			return -3; //source file error.
		}
		Py_XDECREF(pArg);
		Py_XDECREF(pConf);
		
	}
	Py_XDECREF(m_pTypeObj);
	return 0;
}

//////////////////////////////////////////////////////////////////////////
bool	CSphSource_Python::IterateKillListStart ( CSphString & )			
{ 
	if (!m_pInstance)
		return false;

	Py_XDECREF(m_pKillList);

	PyObject* pArgs = NULL;
	PyObject* pFunc = PyObject_GetAttrString(m_pInstance, "GetKillList");
	if (!pFunc || pFunc == Py_None ||!PyCallable_Check(pFunc)) {
		PyErr_Clear(); //GetKillList is a optional feature.
		return false; 
	}

	m_pKillList = PyEval_CallObject(pFunc, pArgs);    
	Py_XDECREF(pArgs);
	Py_XDECREF(pFunc);

	if(PyErr_Occurred()) PyErr_Print();
	PyErr_Clear();

	if(!m_pKillList) 
	{
		this->m_sError = ("Exception happens in python source.(GetKillList)\n");
		return false;
	}
	if(!PyList_Check(m_pKillList)) {
		this->m_sError = "Feed list object to schema.";
		return false;
	}

	m_iKillListSize = (int)PyList_Size(m_pKillList);
	m_iKillListPos = 0;

	return true; 
}

bool	CSphSource_Python::IterateKillListNext ( SphDocID_t & aID)			
{ 
	if( !m_pKillList )
		return false;
	if( m_iKillListPos >= m_iKillListSize )
		return false;

	PyObject* item = PyList_GetItem(m_pKillList,m_iKillListPos);
	if(PyInt_Check(item))
	{
		aID = PyInt_AsLong(item);
	}
	if(PyLong_Check(item)){

#if USE_64BIT
		aID  =  PyLong_AsUnsignedLongLong(item);
#else
		aID = (SphDocID_t)(PyLong_AsLong(item));
#endif

	}
	m_iKillListPos ++; //move next
	return true; 
}

//////////////////////////////////////////////////////////////////////////

void CSphSource_Python::Error ( const char * sTemplate, ... )
{
	if ( !m_sError.IsEmpty() )
		return;

	va_list ap;
	va_start ( ap, sTemplate );
	m_sError.SetSprintf( sTemplate, ap );
	va_end ( ap );
}

PyObject* GetObjectAttr(PyObject *pInst, char* name) //+1
{
	PyObject* item = PyObject_GetAttrString(pInst, name); 
	if(item)
		return item;
	PyObject* pFunc = PyObject_GetAttrString(pInst, "__getattr__");
	if(!pFunc)
		return NULL;
	PyObject* pArgsKey  = Py_BuildValue("(s)",name);
	PyObject* pResult = PyEval_CallObject(pFunc, pArgsKey);
	Py_XDECREF(pArgsKey);
	Py_XDECREF(pFunc);
	return pResult;
}

#endif
