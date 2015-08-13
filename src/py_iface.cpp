#include "sphinx.h"
#include "sphinxutils.h"
#include "py_layer.h"
#include "py_iface.h"
#include "py_csft.h"
#include "py_source2.h"

#if	_WIN32
#pragma warning( push )
#pragma warning( disable : 4127)
#pragma warning( disable : 4189)
#pragma warning( disable : 4273)
#pragma warning( disable : 4100)
#pragma warning( disable : 4800)
#pragma warning( disable : 4510)
#pragma warning( disable : 4702)
#pragma warning( disable : 4706)
#include "py_csft.cxx"

#pragma warning( pop )
#endif // _WIN32

// get string
#define LOC_GETS(_arg,_key) \
    if ( hSource.Exists(_key) ) \
        _arg = hSource[_key].strval();

// get array of strings
#define LOC_GETAS(_arg,_key) \
    for ( CSphVariant * pVal = hSource(_key); pVal; pVal = pVal->m_pNext ) \
        _arg.Add ( pVal->cstr() );

#define LOC_CHECK_RET_FALSE(_hash,_key,_msg,_add) \
    if (!( _hash.Exists ( _key ) )) \
    { \
        fprintf ( stdout, "ERROR: key '%s' not found " _msg "\n", _key, _add ); \
        return false; \
    }

uint32_t getConfigValues(const CSphConfigSection & hSource, const char* sKey, CSphVector<CSphString>& values){
    // hSource reference as the following.. (hidden)
    int orig_length = values.GetLength();
    LOC_GETAS(values, sKey);
    return values.GetLength() - orig_length;
}
//------ Python Data Source Block -------

typedef enum columnType_ {
    ECT_INTEGER     = 1510976267,
    ECT_TIMESTAMP   = 2782324286,
    ECT_BOOLEAN     = 2324007016,
    ECT_FLOAT       = 3383058069,
    ECT_LONG        = 999795048,
    ECT_STRING      = 2663297705,
    ECT_POLY2D      = 754820778,
    ECT_FIELD       = 1542800728,
    ECT_JSON        = 1795630405,
    ECT_NONE        = 0
} columnType;

void initColumnInfo(CSphColumnInfo& info, const char* sName, const char* sType){
    info.m_sName = sName;
    //info.m_eAttrType
    uint32_t iType = 0;
    if(sType)
      iType = sphCRC32((const BYTE*)sType);
    switch(iType)
    {
    case ECT_INTEGER: info.m_eAttrType = SPH_ATTR_INTEGER; break;
    case ECT_TIMESTAMP: info.m_eAttrType = SPH_ATTR_TIMESTAMP; break;
    case ECT_BOOLEAN: info.m_eAttrType = SPH_ATTR_BOOL; break;
    case ECT_FLOAT: info.m_eAttrType = SPH_ATTR_FLOAT; break;
    case ECT_LONG: info.m_eAttrType = SPH_ATTR_BIGINT; break;
    case ECT_STRING: info.m_eAttrType = SPH_ATTR_STRING; break;
    case ECT_POLY2D: info.m_eAttrType = SPH_ATTR_POLY2D; break;
    case ECT_JSON: info.m_eAttrType = SPH_ATTR_JSON; break;

    case ECT_FIELD:
    case ECT_NONE:
    default:
        info.m_eAttrType = SPH_ATTR_NONE; break;
    };
}

void setColumnBitCount(CSphColumnInfo& tCol, int iBitCount){
    tCol.m_tLocator.m_iBitCount = iBitCount;
}

int  getColumnBitCount(CSphColumnInfo& tCol) {
    return tCol.m_tLocator.m_iBitCount;
}

void setColumnAsMVA(CSphColumnInfo& tCol, bool bJoin) {
    if(tCol.m_eAttrType == SPH_ATTR_INTEGER) {
        tCol.m_eAttrType = SPH_ATTR_UINT32SET;
        tCol.m_eSrc = (bJoin) ? SPH_ATTRSRC_QUERY:SPH_ATTRSRC_FIELD;
    }

    if(tCol.m_eAttrType == SPH_ATTR_BIGINT) {
        tCol.m_eAttrType = SPH_ATTR_INT64SET;
        tCol.m_eSrc = (bJoin) ? SPH_ATTRSRC_QUERY:SPH_ATTRSRC_FIELD;
    }
}

int addFieldColumn(CSphSchema* pSchema, CSphColumnInfo& tCol)
{
    if(pSchema){
        pSchema->m_dFields.Add(tCol);
        return pSchema->m_dFields.GetLength()-1;
    }
    return -1;
}

CSphColumnInfo* getSchemaField(CSphSchema* pSchema, int iIndex){
    if(pSchema){
        if(iIndex>=0 && iIndex<pSchema->m_dFields.GetLength())
            return &(pSchema->m_dFields[iIndex]);
    }
    return NULL;
}

int  getSchemaFieldCount(CSphSchema* pSchema)
{
    if(pSchema){
        return pSchema->m_dFields.GetLength();
    }
    return 0;
}

uint32_t getCRC32(const char* data, size_t iLength)
{
    return sphCRC32((const BYTE*)data, iLength);
}


//------- warp objects -----
int  PySphMatch::getAttrCount()
{
    CSphSource_Python2* pSource = (CSphSource_Python2*) _s;
    return pSource->m_tSchema.GetAttrsCount();
}

int  PySphMatch::getFieldCount()
{
    CSphSource_Python2* pSource = (CSphSource_Python2*) _s;
    return getSchemaFieldCount(&pSource->m_tSchema);
}

void PySphMatch::setAttr ( int iIndex, SphAttr_t uValue ) {
    CSphSource_Python2* pSource = (CSphSource_Python2*) _s; //support
    const CSphColumnInfo & tAttr = pSource->m_tSchema.GetAttr(iIndex);
    _m->SetAttr ( tAttr.m_tLocator, uValue);
}

void PySphMatch::setAttrInt64( int iIndex, int64_t uValue ) {
    CSphSource_Python2* pSource = (CSphSource_Python2*) _s; //support
    const CSphColumnInfo & tAttr = pSource->m_tSchema.GetAttr(iIndex);
    _m->SetAttr ( tAttr.m_tLocator, uValue);
}

void PySphMatch::setAttrFloat ( int iIndex, float fValue ) {
    CSphSource_Python2* pSource = (CSphSource_Python2*) _s; //support
    const CSphColumnInfo & tAttr = pSource->m_tSchema.GetAttr(iIndex);
    _m->SetAttr ( tAttr.m_tLocator, fValue);
}

int PySphMatch::pushMva( int iIndex, std::vector<int64_t>& values, bool bMva64) {
    CSphVector < DWORD > & dMva = _s->m_dMva;
    CSphSource_Python2* pSource = (CSphSource_Python2*) _s; //support
    assert ( dMva.GetLength() );

    const CSphColumnInfo & tAttr = pSource->m_tSchema.GetAttr(iIndex);
    if ( tAttr.m_eAttrType == SPH_ATTR_UINT32SET || tAttr.m_eAttrType == SPH_ATTR_INT64SET)
    {
        //reverify column.
        if(tAttr.m_eAttrType == SPH_ATTR_INT64SET)
            bMva64 = true; //force on.
        _m->SetAttr ( tAttr.m_tLocator, 0);

        if ( tAttr.m_eSrc == SPH_ATTRSRC_FIELD ) {
            int uOff = dMva.GetLength();
            dMva.Add ( 0 ); // reserve value for count
            // for each
            for(std::vector<int64_t>::iterator it = values.begin(); it != values.end(); ++it)
            {
                int64_t d64Val = *it;
                if ( !bMva64 )
                    dMva.Add ( (DWORD) d64Val);
                else
                    sphAddMva64 ( dMva, d64Val );
            }
            int iCount = dMva.GetLength()-uOff-1;
            if ( !iCount )
            {
                dMva.Pop(); // remove reserved value for count in case of 0 MVAs
                //return 0;
                uOff = 0;
            } else
            {
                dMva[uOff] = iCount;
                //return uOff; // return offset to ( count, [value] )
            }
            _m->SetAttr ( tAttr.m_tLocator, uOff );
        }
    }
    return 0; // uOff;
}

void PySphMatch::setAttrString( int iIndex, const char* s)
{
    CSphSource_Python2* pSource = (CSphSource_Python2*) _s;
    pSource->m_dStrAttrs[iIndex] = s;
}

void PySphMatch::setField( int iIndex, const char* utf8_str)
{
    CSphSource_Python2* pSource = (CSphSource_Python2*) _s;
    pSource->m_dFields[iIndex] = (BYTE *)utf8_str;
}

//------------------------------------------------

#if USE_PYTHON
bool SpawnSourcePython2 ( const CSphConfigSection & hSource, const char * sSourceName, CSphSource** pSrcPython)
{
    assert ( hSource["type"]=="python" );

    LOC_CHECK_RET_FALSE ( hSource, "name", "in source '%s'.", sSourceName ); //-> should move to setup.

    CSphString	PySourceName;
    LOC_GETS(PySourceName, "name");

    *pSrcPython = NULL;
    CSphSource_Python2* pSource = (CSphSource_Python2*)createPythonDataSourceObject( sSourceName, PySourceName.cstr() );
    if(!pSource) {
        fprintf ( stdout, "ERROR: Create Python data source failure.\n");
        return false;
    }
    if ( !pSource->Setup ( hSource ) ) {
        if(pSource->GetErrorMessage().Length())
            fprintf ( stdout, "ERROR: %s\n", pSource->GetErrorMessage().cstr());
        SafeDelete ( pSource );
        return false;
    }
    *pSrcPython = pSource;
    return true;
}
#endif

//------ Python Cache Block -------



