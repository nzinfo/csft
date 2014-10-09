# -*- coding: UTF-8 -*-

cimport pycsft
cimport cpython.ref as cpy_ref
from cpython.ref cimport Py_INCREF, Py_DECREF, Py_XDECREF
from cpython.exc cimport PyErr_Fetch, PyErr_Restore

import os
from libcpp cimport bool

from cpython.ref cimport Py_INCREF, Py_XINCREF, Py_DECREF, Py_XDECREF, Py_CLEAR
from libc.stdint cimport uint32_t, uint64_t, int64_t
from libc.stdio cimport printf
from libcpp.vector cimport vector

import traceback

"""
    定义
        Python 数据源
        Python 分词法
        Python 配置服务
        Python Cache 的 C++ <-> Python 的调用接口
"""


#test gc
import gc

# Ref: http://stackoverflow.com/questions/1176136/convert-string-to-python-class-object
def __findPythonClass(sName):
    import importlib
    pos = sName.find('.')
    module_name = sName[:pos]
    cName = sName[pos+1:]
    #print module_name, cName
    # import os
    # print 'cwd: ', os.getcwd()
    # print '@@@@@@@@@@@@@@@@@@@@@@@@@@@@'
    try:
        m = importlib.import_module(module_name)
        #print m, dir(m)
        c = getattr(m, cName)
        return c
    except ImportError, e:
        print e
        return None


# Cython creator API
# 设置类库的加载路径
cdef public api void __setPythonPath(const char* sPath):
    import sys
    sPaths = [x.lower() for x in sys.path]
    sPath_ = os.path.abspath(sPath)
    if sPath_ not in sPaths:
        sys.path.append(sPath_)
    #print __findPythonClass('flask.Flask')

# 根据类的名称 加载 Python 的 类型对象
cdef public api cpy_ref.PyObject* __getPythonClassByName(const char* class_name):
    import sys
    sName = class_name
    clsType = __findPythonClass(sName)
    if clsType:
        return ( <cpy_ref.PyObject*>clsType )
    else:
        return NULL

"""
    Import C++ Interface
"""
cdef extern from "sphinxstd.h":
    cdef cppclass CSphString:
        const char * cstr () const

## --- python conf ---
cdef extern from "pyiface.h":
    cdef cppclass CSphStringList:
        int GetLength () const
        void Reset ()
        const CSphString & operator [] ( int iIndex ) const

    cpdef cppclass PySphConfig:
        bool hasSection(const char* sType, const char* sName)
        bool addSection(const char* sType, const char* sName)
        bool addKey(const char* sType, const char* sName, const char* sKey, char* sValue)

    cdef cppclass IConfProvider:
        pass

    cdef cppclass ConfProviderWrap:
        #PyObject *obj
        ConfProviderWrap(cpy_ref.PyObject *obj)

    uint32_t getConfigValues(const CSphConfigSection & hSource, const char* sKey, CSphStringList& value)

cdef extern from "sphinxutils.h":
    cdef cppclass CSphConfigSection:
        void IterateStart () const
        bool IterateStart ( const CSphString & tKey ) const
        bool IterateNext () const
        const CSphString & IterateGetKey () const

## --- python source ---

cdef extern from "sphinx.h":
    ctypedef SphDocID_t
    ctypedef SphWordID_t
    ctypedef Hitpos_t
    ctypedef SphAttr_t


    ctypedef enum ESphAttr:
        SPH_ATTR_NONE
        SPH_ATTR_INTEGER
        SPH_ATTR_TIMESTAMP
        SPH_ATTR_ORDINAL
        SPH_ATTR_BOOL
        SPH_ATTR_FLOAT
        SPH_ATTR_BIGINT
        SPH_ATTR_STRING
        SPH_ATTR_WORDCOUNT
        SPH_ATTR_POLY2D
        SPH_ATTR_STRINGPTR
        SPH_ATTR_TOKENCOUNT
        SPH_ATTR_JSON
        SPH_ATTR_UINT32SET
        SPH_ATTR_INT64SET

    cdef cppclass CSphColumnInfo:
        #CSphColumnInfo ( const char * sName=NULL, ESphAttr eType=SPH_ATTR_NONE )
        CSphString  m_sName
        ESphAttr    m_eAttrType
        bool        m_bIndexed
        int         m_iIndex

    cdef cppclass CSphSchema:
        int	GetFieldIndex ( const char * sName ) const
        int	GetAttrIndex ( const char * sName ) const

        void	Reset ()
        void    ResetAttrs ()
        int	GetRowSize () const
        int	GetStaticSize () const
        int	GetDynamicSize () const
        int	GetAttrsCount () const

        const CSphColumnInfo &	GetAttr ( int iIndex ) const
        const CSphColumnInfo *	GetAttr ( const char * sName ) const
        void  AddAttr ( const CSphColumnInfo & tAttr, bool bDynamic )

    cdef cppclass CSphSource:
        pass

    # build index needs helper interface
    cdef cppclass ISphHits:
        int Length () const
        void AddHit ( SphDocID_t uDocid, SphWordID_t uWordid, Hitpos_t uPos )

    cdef cppclass CSphMatch:
        pass

cdef extern from "sphinxutils.h":
    cdef cppclass CSphConfigSection:
        void IterateStart () const
        bool IterateStart ( const CSphString & tKey ) const
        bool IterateNext () const
        const CSphString & IterateGetKey () const

cdef extern from "pyiface.h":

    cdef uint32_t getCRC32(const char* data, size_t iLength)

    void initColumnInfo(CSphColumnInfo& info, const char* sName, const char* sType)
    void setColumnBitCount(CSphColumnInfo& info, int iBitCount)
    int  getColumnBitCount(CSphColumnInfo& info)
    void setColumnAsMVA(CSphColumnInfo& info, bool bJoin)
    int addFieldColumn(CSphSchema* pSchema, CSphColumnInfo& info)
    int  getSchemaFieldCount(CSphSchema* pSchema)
    CSphColumnInfo* getSchemaField(CSphSchema* pSchema, int iIndex)

    cdef cppclass PySphMatch:
        void bind(CSphSource* s, CSphMatch* _m)

        void setDocID(uint64_t id)
        uint64_t getDocID()

        int  getAttrCount()
        int  getFieldCount()

        void setAttr ( int iIndex, SphAttr_t uValue )
        void setAttrFloat ( int iIndex, float fValue )
        void setAttrInt64( int iIndex, int64_t uValue )

        void pushMva( int iIndex, vector[int64_t]& values, bool bMva64)
        void setAttrString( int iIndex, const char* s)
        void setField( int iIndex, const char* utf8_str)

cdef extern from "pysource.h":
    cdef cppclass CSphSource_Python2:
        CSphSource_Python2 ( const char * sName, cpy_ref.PyObject* obj)
        CSphMatch   m_tDocInfo
        ISphHits    m_tHits #protected
        ISphHits *  getHits ()


## --- python tokenizer ---

## --- python cache ---

## --- python query ---

"""
    Define Python Wrap, for Python side.
    Wrap the python code interface
    -> after import c++ class, we needs build python wrap... silly.
"""

## --- python conf ---
cdef class PySphConfigWrap(object):
    cdef PySphConfig* conf_
    def __init__(self):
        self.conf_ = NULL

    cdef init_wrap(self, PySphConfig* conf):
        self.conf_ = conf

    # FIXME: should raise an exception.
    def hasSection(self, sType, sName):
        if self.conf_:
            return self.conf_.hasSection(sType, sName)
        return False

    def addSection(self, sType, sName):
        if self.conf_:
            return self.conf_.addSection(sType, sName)
        return False

    # if not section section, just return false.
    def addKey(self, sType, sName, sKey, sValue):
        if self.conf_:
            return self.conf_.addKey(sType, sName, sKey, sValue)
        return False

## --- python source ---
def attr_callable(obj, attr_name):
    try:
        func = getattr(obj, attr_name)
        if callable(func):
            return True
        else:
            print("[WARNING][PySource] '%s' is defined but not a callable function." % attr_name);
    except AttributeError, ex:
        return False
    return False

class InvalidAttributeType(Exception):
    pass

cdef class PySchemaWrap(object):
    """
        用于向 Python 端 提供操作 Schema 的接口
    """
    cdef CSphSchema* _schema
    cdef object _valid_attribute_type # python list
    cdef int iIndex
    cdef list _join_fields
    cdef int _i_plain_fields_length

    def  __cinit__(self):
        self._schema = NULL
        self._valid_attribute_type = ["integer", "timestamp", "boolean", "float", "long", "string", "poly2d", "field", "json"]
        self.iIndex = 0
        self._join_fields = []
        self._i_plain_fields_length = 0

    cdef sphColumnInfoTypeToString(self, CSphColumnInfo& tCol):
        type2str = {
            SPH_ATTR_NONE:"none",
            SPH_ATTR_INTEGER:"integer",
            SPH_ATTR_TIMESTAMP:"timestamp",
            SPH_ATTR_ORDINAL:"str2ord",
            SPH_ATTR_BOOL:"boolean",
            SPH_ATTR_FLOAT:"float",
            SPH_ATTR_BIGINT:"long",
            SPH_ATTR_STRING:"string",
            SPH_ATTR_WORDCOUNT:"wordcount",
            SPH_ATTR_POLY2D:"poly2d",
            SPH_ATTR_STRINGPTR:"stringPtr",
            SPH_ATTR_TOKENCOUNT:"tokencount",
            SPH_ATTR_JSON:"json",
            SPH_ATTR_UINT32SET:"mva32",
            SPH_ATTR_INT64SET:"mva64",
        }
        if tCol.m_eAttrType in type2str:
            return type2str[tCol.m_eAttrType]

        return "unknown"

    cdef bind(self, CSphSchema* s):
        self._schema = s

    cpdef done(self):
        cdef CSphColumnInfo tCol
        for sName in self._join_fields:
            initColumnInfo(tCol, sName, NULL)
            tCol.m_iIndex = -1
            tCol.m_bIndexed = True
            addFieldColumn(self._schema, tCol)
        self._join_fields = []

    cpdef int addAttribute(self, const char* sName, const char* sType, int iBitSize=0, bool bJoin=False, bool bIsSet=False):
        """
            向实际的 Schema 中增加 新字段
            @iBitSize <= 0, means standand size.

            - check sType
        """
        cdef CSphColumnInfo tCol

        if sType not in self._valid_attribute_type:
            raise InvalidAttributeType()
        if sType == str("field"):
            raise InvalidAttributeType() # used addField plz.

        initColumnInfo(tCol, sName, sType);
        tCol.m_iIndex = self.iIndex
        self.iIndex += 1
        if iBitSize:
            setColumnBitCount(tCol, iBitSize)
        # Patch on MVA
        if bIsSet:
            setColumnAsMVA(tCol, bJoin)

        self._schema.AddAttr(tCol, True)
        return tCol.m_iIndex

    cpdef addField(self, const char* sName, bool bJoin=False):
        """
            向 Schema 添加全文检索字段
        """
        cdef CSphColumnInfo tCol
        initColumnInfo(tCol, sName, NULL);
        # int	m_iIndex;  ///< index into source result set (-1 for joined fields)
        if not bJoin:
            tCol.m_iIndex = self.iIndex
            tCol.m_bIndexed = True
            self.iIndex += 1
            self._i_plain_fields_length += 1 # add new plain field.

            return addFieldColumn(self._schema, tCol)
        else:
            # TODO: add new field to a list, and add to m_dFields...
            self._join_fields.append(sName)
            return getSchemaFieldCount(self._schema) + len(self._join_fields) -1

    cpdef int fieldsBaseCount(self):
        return self._i_plain_fields_length;

    cpdef int fieldsCount(self):
        return getSchemaFieldCount(self._schema) + len(self._join_fields) # the total count.

    cpdef int attributeCount(self):
        return self._schema.GetAttrsCount()

    cpdef object fieldsInfo(self, int iIndex):
        cdef CSphColumnInfo* tCol
        tCol = getSchemaField(self._schema, iIndex)
        if tCol:
            # FIXME: add wordpart info.
            return {
                "name":tCol.m_sName.cstr(),
            }
        #FIXME: the join fields.
        return None

    cpdef object attributeInfo(self, int iIndex):
        cdef CSphColumnInfo tCol
        if iIndex>= 0 and iIndex <= self.attributeCount():
            tCol = self._schema.GetAttr(iIndex)
            return {
                "name":tCol.m_sName.cstr(),
                "type":self.sphColumnInfoTypeToString(tCol),
                "index":tCol.m_iIndex,
                "bit": getColumnBitCount(tCol)
            }
        return None

    cpdef int   getFieldIndex(self, const char* sKey):
        return self._schema.GetFieldIndex(sKey)

    cpdef int   getAttributeIndex(self, const char* sKey):
        return self._schema.GetAttrIndex(sKey)

cdef class PyDocInfo(object):
    """
        供 Python 程序 设置 待检索文档的属性 和 全文检索字段
    """
    cdef PySphMatch _docInfo
    cdef int _iAttrCount
    cdef int _iFieldCount

    cdef void bind(self, CSphSource_Python2* pSource, CSphMatch* docInfo):
        self._docInfo.bind(<CSphSource *>pSource, docInfo)
        self._iAttrCount = self._docInfo.getAttrCount()
        self._iFieldCount = self._docInfo.getFieldCount()

    cpdef setDocID(self, uint64_t id):
        self._docInfo.setDocID(id)

    cpdef uint64_t getDocID(self):
        return self._docInfo.getDocID()

    cpdef int setAttr(self, int iIndex, SphAttr_t v):
        if iIndex < 0 or iIndex>= self._iAttrCount: raise IndexError()
        self._docInfo.setAttr(iIndex, v)
        return 0

    cpdef int setAttrFloat(self, int iIndex, float v):
        if iIndex < 0 or iIndex>= self._iAttrCount: raise IndexError()
        self._docInfo.setAttrFloat(iIndex, v)
        return 0

    cpdef int setAttrInt64(self, int iIndex, int64_t v):
        if iIndex < 0 or iIndex>= self._iAttrCount: raise IndexError()
        self._docInfo.setAttrInt64(iIndex, v)
        return 0

    cpdef int setAttrTimestamp(self, int iIndex, int64_t dVal):
        cdef int64_t v
        #print iIndex, self._iAttrCount, dVal, self._docInfo.getAttrCount()
        if iIndex < 0 or iIndex>= self._iAttrCount: raise IndexError()
        #Python is returning the time since the epoch in seconds. Javascript takes the time in milliseconds.
        self._docInfo.setAttrInt64(iIndex, dVal)
        return 0

    cpdef int setAttrMulti(self, int iIndex, list values, bool bValue64 = False):
        cdef vector[int64_t] vect
        if iIndex < 0 or iIndex>= self._iAttrCount: raise IndexError()
        vect.reserve(1024)
        for v in values:
            vect.push_back(v)
        self._docInfo.pushMva(iIndex, vect, bValue64)
        return len(values)


    cpdef int setAttrString(self, int iIndex, const char* sVal):
        #printf("got str %s.\n", sVal);
        if iIndex < 0 or iIndex>= self._iAttrCount: raise IndexError()
        self._docInfo.setAttrString(iIndex, sVal)
        return 0

    cpdef int setField(self, int iIndex, const char* sVal):
        if iIndex < 0 or iIndex>= self._iFieldCount: raise IndexError()
        self._docInfo.setField(iIndex, sVal)
        return 0

    cpdef uint64_t getLastDocID(self): #FIXME: larger this when docid lager than 64bit.
        return 0

cdef class PyHitCollector(object):
    """
        为 Python 程序提供在索引建立阶段使用 的 Hit 采集接口, 可以手工设置 FieldIndex
    """
    cdef ISphHits* _hits
    cdef void bind(self, ISphHits* hits):
        self._hits = hits;

    cpdef uint64_t getPrevDocID(self):
        return 0

    cpdef uint64_t getDocID(self):
        return 0



cdef class PySourceWrap(object):
    """
        C++ -> Python 的桥; 额外提供
          - DocInfo 让 Python 修改 Document 的属性信息
          - HitCollector 让 Python 可以主动推送索引
    """
    cdef object _pysource
    cdef PyDocInfo _docInfo
    cdef PyHitCollector _hitCollecotr
    cdef CSphSource_Python2* _csrc
    # kill list related.
    cdef list _killList
    cdef int  _killListPos

    def __init__(self, pysrc):
        self._pysource = pysrc
        self._docInfo = PyDocInfo()
        self._hitCollecotr = PyHitCollector()
        self._csrc = NULL
        self._killList = None
        self._killListPos = 0

    cdef bindSource(self, CSphSource_Python2* pSource):
        """
            绑定 DocInfo & HitCollector 到指定的 DataSource,
                - do real bind after setup.
        """
        self._csrc = pSource;
        #printf("cpp source: %p\n", pSource)

    cpdef int setup(self, source_conf):
        try:
            ret = self._pysource.setup(source_conf)
            if ret or ret == None:
                #check obj has necessary method.
                if not attr_callable(self._pysource, 'feed'):
                    return -2

                return 0
        except Exception, ex:
            traceback.print_exc()
            return -1 # setup failured.

        return -100 # some error happen

    cpdef int connect(self, schema):
        # check have the function
        if attr_callable(self._pysource, 'connect'):
            try:
                ret = self._pysource.connect(schema)
                if ret or ret == None:
                    #bind source.
                    self._docInfo.bind(self._csrc, &(self._csrc.m_tDocInfo) )
                    self._hitCollecotr.bind(self._csrc.getHits())
                    return 0
                else:
                    return -2
            except Exception, ex:
                traceback.print_exc()
                return -1 # setup failured.
        return 0 # no such define

    cpdef int indexFinished(self):
        # optinal call back.
        if attr_callable(self._pysource, 'indexFinished'):
            try:
                ret = self._pysource.indexFinished()
                if ret or ret == None:
                    return 0
                else:
                    return -2
            except Exception, ex:
                traceback.print_exc()
                return -1 # some error in python code.
        return 0 # no such define

    cpdef int beforeIndex(self):
        # optinal call back.
        if attr_callable(self._pysource, 'beforeIndex'):
            try:
                ret = self._pysource.beforeIndex()
                if ret or ret == None: # treat None as True
                    return 0
                else:
                    return -2
            except Exception, ex:
                traceback.print_exc()
                return -1 # some error in python code.
        return 0 # no such define

    cpdef int afterIndex(self, bool bNormalExit):
        # optinal call back.
        if attr_callable(self._pysource, 'afterIndex'):
            try:
                ret = self._pysource.afterIndex(bNormalExit)
                if ret or ret == None:
                    return 0
                else:
                    return -2
            except Exception, ex:
                traceback.print_exc()
                return -1 # some error in python code.
        return 0 # no such define

    cpdef int index_finished(self):
        if attr_callable(self._pysource, 'indexFinished'):
            try:
                ret = self._pysource.indexFinished()
                if ret or ret == None:
                    return 0
                else:
                    return -2
            except Exception, ex:
                traceback.print_exc()
                return -1 # some error in python code.
        return 0 # no such define

    cpdef int getJoinField(self, const char* attrName):
        # programal optional, if has join field , the method must define.
        if attr_callable(self._pysource, 'feedJoinField'):
            try:
                ret = self._pysource.feedJoinField(attrName, self._docInfo, self._hitCollecotr)
                if ret or ret == None:
                    return 0
            except Exception, ex:
                traceback.print_exc()
                return -1 # some error in python code.
        return 0

    cpdef getJoinMva(self, const char* attrName):
        # programal optional, if has list-query , the method must define.
        if attr_callable(self._pysource, 'getMultiValueAttribute'):
            try:
                ret = self._pysource.getMultiValueAttribute(attrName)
                if ret:
                    return ret
                else:
                    return (0, 0)
            except Exception, ex:
                traceback.print_exc()
                return -1 # some error in python code.
        return (0,0) # no such define

    cpdef int next(self):
        # should check this function when binding
        try:
            if self._pysource.feed(self._docInfo, self._hitCollecotr): # must return True | some value, return None | False will stop indexing.
                return 0
            else:
                return 1
        except Exception, ex:
            traceback.print_exc()
            return -1 # some error in python code.

    cpdef int getKillList(self):
        # optinal call back.
        if attr_callable(self._pysource, 'getKillList'):
            try:
                klist = self._pysource.getKillList()
                if klist or klist == None:
                    self._killList = klist if klist else []
                    self._killListPos = 0
                    return 0
                else:
                    return -2
            except Exception, ex:
                traceback.print_exc()
                return -1 # some error in python code.
        return 0 # no such define

    cdef int getKillListItem(self, uint64_t* opDocID):
        if self._killListPos >= len(self._killList):
            return -1

        if opDocID:
            opDocID[0] = self._killList[self._killListPos]

        self._killListPos += 1
        return 0

## --- python tokenizer ---

## --- python cache ---

## --- python query ---

"""
    Define Python Wrap , for CPP side.
"""

## --- python conf ---
cdef class PyConfProviderWrap:
    cdef ConfProviderWrap* _p
    cdef cpy_ref.PyObject* _pyconf
    def __init__(self, pyConfObj):
        self._pyconf = <cpy_ref.PyObject*>pyConfObj # the user customed config object.
        self._p = new ConfProviderWrap(<cpy_ref.PyObject*>self)

    cdef int process(self, PySphConfig & hConf):
        cdef int nRet = 0
        _pyconf = <object>self._pyconf
        hConfwrap = PySphConfigWrap(); hConfwrap.init_wrap(&hConf)
        nRet = _pyconf.process(hConfwrap)
        if nRet == None:
            return 0
        return int(nRet)

## --- python source ---

## --- python tokenizer ---

## --- python cache ---

## --- python query ---


"""
    Python Object's C API
"""

## --- python conf ---

cdef public int py_iconfprovider_process(void *ptr, PySphConfig& hConf):
    #gc.collect()
    cdef PyConfProviderWrap self = <PyConfProviderWrap>(ptr)
    #import sys
    #print sys.getrefcount(self)
    return self.process(hConf)

## --- python source ---
# 处理配置文件的读取, 读取 配置到  key -> value; key-> valuelist.
cdef public int py_source_setup(void *ptr, const CSphConfigSection & hSource):
    cdef const char* key
    cdef CSphStringList values
    cdef uint32_t value_count
    cdef dict conf_dict
    conf_dict = wrap_sphinx_config(hSource)
    # build helper object & feed data in it.
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    # call wrap
    return self.setup(conf_dict)

    # temp usage for crc32 key ------->
    if False:
        keys = ["integer", "timestamp", "boolean", "float", "long", "string", "poly2d", "field", "json"]
        for k in keys:
            print k, getCRC32(k, len(k))
    #print conf_items

# - [Renamed] GetSchema -> buildSchema  @Deprecated
#cdef public int py_source_build_schema(void *ptr, PySphConfig& hConf):
#    pass

# - Connected
cdef public int py_source_connected(void *ptr, CSphSchema& Schema):
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    pySchema = PySchemaWrap()
    pySchema.bind(&Schema)
    return self.connect(pySchema)

# - OnIndexFinished
cdef public int py_source_index_finished(void *ptr):
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    return self.index_finished()

# - OnBeforeIndex
cdef public int py_source_before_index(void *ptr):
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    return self.beforeIndex()

# - GetDocField
cdef public int py_source_get_join_field(void *ptr, const char* fieldname):
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    return self.getJoinField(fieldname)

# - GetMVAValue
cdef public int py_source_get_join_mva(void *ptr, const char* fieldname, uint64_t* opDocID, int64_t* opVal):
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    docid, v = self.getJoinMva(fieldname)
    if opDocID and opVal:
        opDocID[0] = docid
        opVal[0] = v
        return 0
    return -1;

# - NextDocument
cdef public int py_source_next(void *ptr):
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    return self.next()

# - OnAfterIndex
cdef public int py_source_after_index(void *ptr, bool bNormalExit):
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    return self.afterIndex(bNormalExit)

# - GetKillList
cdef public int py_source_get_kill_list(void *ptr):
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    return self.getKillList()

cdef public int py_source_get_kill_list_item(void *ptr, uint64_t* opDocID):
    cdef PySourceWrap self = <PySourceWrap>(ptr)
    return self.getKillListItem(opDocID)

# - [Removed] GetFieldOrder -> 在 buildSchema 统一处理
# - [Removed] BuildHits -> 有 TokenPolicy 模块处理

## --- python tokenizer ---

## --- python cache ---

## --- python query ---


"""
    Object creation function.
"""

## --- python conf ---

cdef public api IConfProvider* createPythonConfObject(const char* class_name, const CSphConfigSection & conf_python_section):
    cdef PyConfProviderWrap pyconf
    cdef dict conf_dict
    #cdef cpy_ref.PyObject* ptr

    conf_dict = wrap_sphinx_config(conf_python_section)

    sName = class_name
    clsType = __findPythonClass(sName)
    if clsType:
        obj = clsType(conf_dict)
        pyconf = PyConfProviderWrap(obj)
        Py_XINCREF(<cpy_ref.PyObject*>pyconf)  
        return <IConfProvider*>(pyconf._p)
    else:
        return NULL # provider not found.

cdef public api void destoryPythonConfObject(cpy_ref.PyObject*  ptr):
    #import sys
    #print sys.getrefcount(<object>ptr),'0000\n\n\n'
    Py_CLEAR(ptr); 
    return

# 处理配置文件的读取, 读取 配置到  key -> value; key-> valuelist.
cdef dict wrap_sphinx_config(const CSphConfigSection & hSource):
    cdef const char* key
    cdef CSphStringList values
    cdef uint32_t value_count

    conf_items = {}
    hSource.IterateStart()
    while hSource.IterateNext():
        values.Reset()
        key = hSource.IterateGetKey().cstr()
        value_count = getConfigValues(hSource, key, values)

        if value_count == 1:
            conf_items[key] = values[0].cstr()
            continue
        v = []
        for i in range(0, values.GetLength()):
            v.append( values[i].cstr() )
        conf_items[key] = v

    # call wrap
    return conf_items

## --- python source ---
# pass source config infomation.
cdef public api CSphSource * createPythonDataSourceObject ( const char* sName, const char * class_name ):
    cdef CSphSource_Python2* pySource

    sName = class_name
    clsType = __findPythonClass(sName)
    #print "hhhhh\n"
    if clsType:
        # Do error report @user code.
        try:
            obj = clsType()
        except Exception, ex:
            traceback.print_exc()
            return NULL

        wrap = PySourceWrap(obj)
        #Py_INCREF(wrap) # pass pyobjct* to cpp code should addref ( @ the cpp code. )

        pySource = new CSphSource_Python2(sName, <cpy_ref.PyObject*>wrap)
        # FIXME: crash when new failure.
        wrap.bindSource(pySource);
        return <CSphSource*>pySource
    else:
        return NULL

## --- python tokenizer ---

## --- python cache ---

## --- python query ---


#end of file
