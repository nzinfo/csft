#ifndef PYSOURCE_H
#define PYSOURCE_H

#include "sphinx.h"
#include "py_layer.h"
#include "py_iface.h"


class CSphSource_Python2 : public CSphSource_Document
{

friend class PySphMatch;

public:

            CSphSource_Python2 ( const char * sName, PyObject *obj);
            ~CSphSource_Python2 ();
    bool	Setup ( const CSphConfigSection & hSource);

    const CSphString &		GetErrorMessage () const	{ return m_sError; }
    ISphHits *              getHits ();

public:
    /// connect to the source (eg. to the database)
    /// connection settings are specific for each source type and as such
    /// are implemented in specific descendants
    virtual bool						Connect ( CSphString & sError ) ;

    /// disconnect from the source
    virtual void						Disconnect () ;

    /// check if there are any attributes configured
    /// note that there might be NO actual attributes in the case if configured
    /// ones do not match those actually returned by the source
    virtual bool						HasAttrsConfigured () ;

    /// check if there are any joined fields
    // virtual bool						HasJoinedFields () { return false; }

    /// begin indexing this source
    /// to be implemented by descendants
    virtual bool						IterateStart ( CSphString & sError ) ;

    /// get next document
    /// to be implemented by descendants
    /// returns false on error
    /// returns true and fills m_tDocInfo on success
    /// returns true and sets m_tDocInfo.m_iDocID to 0 on eof -> by CSphSource_Document
    // virtual bool						IterateDocument ( CSphString & sError ) = 0;

    /// get next hits chunk for current document
    /// to be implemented by descendants
    /// returns NULL when there are no more hits
    /// returns pointer to hit vector (with at most MAX_SOURCE_HITS) on success
    /// fills out-string with error message on failure ->CSphSource_Document
    // virtual ISphHits *					IterateHits ( CSphString & sError ) = 0;

    /// get joined hits from joined fields (w/o attached docinfos)
    /// returns false and fills out-string with error message on failure
    /// returns true and sets m_tDocInfo.m_uDocID to 0 on eof
    /// returns true and sets m_tDocInfo.m_uDocID to non-0 on success ->...?
    virtual ISphHits *					IterateJoinedHits ( CSphString & sError );

    /// begin iterating values of out-of-document multi-valued attribute iAttr
    /// will fail if iAttr is out of range, or is not multi-valued
    /// can also fail if configured settings are invalid (eg. SQL query can not be executed)
    virtual bool						IterateMultivaluedStart ( int iAttr, CSphString & sError ) ;

    /// get next multi-valued (id,attr-value) or (id, offset) for mva64 tuple to m_tDocInfo
    virtual bool						IterateMultivaluedNext () ;

    /// begin iterating values of multi-valued attribute iAttr stored in a field
    /// will fail if iAttr is out of range, or is not multi-valued
    // virtual SphRange_t					IterateFieldMVAStart ( int iAttr ) ;

    /// begin iterating kill list
    virtual bool						IterateKillListStart ( CSphString & sError ) ;

    /// get next kill list doc id
    virtual bool						IterateKillListNext ( SphDocID_t & tDocId ) ;

    /// post-index callback
    /// gets called when the indexing is succesfully (!) over
    virtual void						PostIndex ();

public:
    /// field data getter
    /// to be implemented by descendants
    virtual BYTE **			NextDocument ( CSphString & sError ) ;

protected:
    BYTE *          m_dFields [ SPH_MAX_FIELDS ];
    int             m_iMultiAttr;

    //handle sql_join.
    int					m_iJoinedHitField;	///< currently pulling joined hits from this field (index into schema; -1 if not pulling)
    SphDocID_t			m_iJoinedHitID;		///< last document id
    int					m_iJoinedHitPositions[ SPH_MAX_FIELDS ];	///< last hit position

protected:
    CSphString		m_sError;
    bool          _bAttributeConfigured;
    PyObject    * _obj;

};

#endif // PYSOURCE_H
