/*
    Copyright 2012 Patrick von Reth <vonreth@kde.org>
    Copyright 2008 Jeff Mitchell <mitchell@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library. If not, see <http://www.gnu.org/licenses/>.
*/

//#define _WIN32_DCOM
//#include <comdef.h>

#define INSIDE_WMIQUERY
#include "wmiquery.h"
#include "wmimanager.h"

#ifdef _DEBUG
# pragma comment(lib, "comsuppwd.lib")
#else
# pragma comment(lib, "comsuppw.lib")
#endif
# pragma comment(lib, "wbemuuid.lib")

#include <iostream>
#include <Wbemidl.h>
#include <Oaidl.h>

# pragma comment(lib, "wbemuuid.lib")

#include <QtCore/QDebug>
#include <QtCore/QVariant>
#include <QtCore/QList>
#include <QtCore/QStringList>
#include <QtCore/QCoreApplication>

//needed for mingw
inline OLECHAR* SysAllocString(const QString &s){
    const OLECHAR *olename = reinterpret_cast<const OLECHAR *>(s.utf16());
    return ::SysAllocString(olename);
}

using namespace Solid::Backends::Wmi;


QString bstrToQString(BSTR bstr)
{
    QString str((QChar*)bstr,::SysStringLen(bstr));
    return str;

}

QVariant WmiQuery::Item::msVariantToQVariant(VARIANT msVariant, CIMTYPE variantType)
{
    QVariant returnVariant(QVariant::Invalid);
    if(msVariant.vt == VT_NULL){
        return QVariant(QVariant::String);
    }

    switch(variantType) {
    case CIM_STRING:
    case CIM_CHAR16:
    case CIM_UINT64://bug I get a wrong value from ullVal
    {
        QString str = bstrToQString(msVariant.bstrVal);
        QVariant vs(str);
        returnVariant = vs;

    }
        break;
    case CIM_BOOLEAN:
    {
        QVariant vb(msVariant.boolVal);
        returnVariant = vb;
    }
        break;
    case CIM_UINT8:
    {
        QVariant vb(msVariant.uintVal);
        returnVariant = vb;
    }
        break;
    case CIM_UINT16:
    {
        QVariant vb(msVariant.uintVal);
        returnVariant = vb;
    }
        break;
    case CIM_UINT32:
    {
        QVariant vb(msVariant.uintVal);
        returnVariant = vb;
    }
        break;
        //    case CIM_UINT64:
        //    {
        //        QVariant vb(msVariant.ullVal);
        ////        wprintf(L"ulonglong %d %I64u\r\n",variantType, msVariant.ullVal); // 32-bit on x86, 64-bit on x64
        //        returnVariant = vb;
        //    }
        //        break;
        //    default:
        //        qDebug()<<"Unsupported variant"<<variantType;
    };
    VariantClear(&msVariant);
    return returnVariant;
}

/**
 When a WmiQuery instance is created as a static global
 object a deadlock problem occurs in pLoc->ConnectServer.
 Please DO NOT USE the following or similar statement in
 the global space or a class.

 static WmiQuery instance;
*/

QVariant WmiQuery::Item::getProperty(BSTR bstrProp) const
{
    QVariant result;
    if(m_p == NULL)
        return result;
    VARIANT vtProp;
    CIMTYPE variantType;
    HRESULT hr = m_p->Get(bstrProp, 0, &vtProp, &variantType, 0);
    if (SUCCEEDED(hr)) {
        result = msVariantToQVariant(vtProp,variantType);
    }else{
        QString className = getProperty(L"__CLASS").toString();
        QString qprop = bstrToQString(bstrProp);
        qFatal("\r\n--------------------------------------------------------------\r\n"
               "Error: Property: %s not found in %s\r\n"
               "--------------------------------------------------------------\r\n",qPrintable(qprop),qPrintable(className));
    }
    return result;
}
QVariant WmiQuery::Item::getProperty(const QString &property) const
{
    QVariant result;
    BSTR bstrProp;
    bstrProp = ::SysAllocString(property);
    result = getProperty(bstrProp);
    ::SysFreeString(bstrProp);
    return result;
}

QVariantMap WmiQuery::Item::getAllProperties()
{
    if(m_properies.isEmpty()){
        SAFEARRAY *psaNames;
        HRESULT hr = m_p->GetNames(NULL,  WBEM_FLAG_ALWAYS | WBEM_FLAG_NONSYSTEM_ONLY,NULL,&psaNames);
        if (SUCCEEDED(hr)) {
            long lLower, lUpper;
            SafeArrayGetLBound( psaNames, 1, &lLower );
            SafeArrayGetUBound( psaNames, 1, &lUpper );
            for(long i=lLower;i<lUpper;++i){
                BSTR pName = { 0 };
                hr = SafeArrayGetElement( psaNames, &i, &pName );
                QVariant vr = getProperty(pName);
                if(vr.isValid()){
                    QString key = bstrToQString(pName);
                    m_properies[key] = getProperty(pName);
                }
            }
        }else{
            qWarning()<<"Querying all failed";
        }
        SafeArrayDestroy( psaNames);
    }
    return m_properies;
}

WmiQuery::Item::Item()
    :m_p(NULL)
{
}

WmiQuery::Item::Item(IWbemClassObject *p) : m_p(p)
{
    if(m_p != NULL)
        m_p->AddRef();
}

WmiQuery::Item::Item(const Item& other) : m_p(other.m_p)
{
    if(m_p != NULL)
        m_p->AddRef();
}

WmiQuery::Item& WmiQuery::Item::operator=(const Item& other)
{
    if(m_p != NULL){
        m_p->Release();
        m_p = NULL;
    }
    if(other.m_p != NULL){
        m_p = other.m_p;
        m_p->AddRef();
    }
    return *this;
}

WmiQuery::Item::~Item()
{
    if(m_p != NULL &&
            !(qApp->closingDown() || WmiQuery::instance().m_bNeedUninit))//this means we are in a QApplication, so qt already called CoUninitialize and all COM references are all ready freed
        m_p->Release();
}

IWbemClassObject* WmiQuery::Item::data() const
{
    if(m_p != NULL)
        m_p->AddRef();
    return m_p;
}

bool WmiQuery::Item::isNull() const
{
    return m_p == NULL;
}

WmiQuery::WmiQuery()
    : m_failed(false)
    , pLoc(0)
    , pSvc(0)
{
    HRESULT hres;

    hres =  CoInitializeEx(0,COINIT_MULTITHREADED);
    if( FAILED(hres) && hres != S_FALSE && hres != RPC_E_CHANGED_MODE )
    {
        qCritical() << "Failed to initialize COM library.  Error code = 0x" << hex << quint32(hres) << endl;
        m_failed = true;
    }
    m_bNeedUninit = ( hres != S_FALSE && hres != RPC_E_CHANGED_MODE );
    if( !m_failed )
    {
        hres =  CoInitializeSecurity( NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_DEFAULT,
                                      RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL );

        // RPC_E_TOO_LATE --> security already initialized
        if( FAILED(hres) && hres != RPC_E_TOO_LATE )
        {
            qCritical() << "Failed to initialize security. " << "Error code = " << hres << endl;
            if ( m_bNeedUninit )
                CoUninitialize();
            m_failed = true;
        }
    }
    if( !m_failed )
    {
        hres = CoCreateInstance( CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, IID_IWbemLocator, (LPVOID *) &pLoc );
        if (FAILED(hres))
        {
            qCritical() << "Failed to create IWbemLocator object. " << "Error code = " << hres << endl;
            if ( m_bNeedUninit )
                CoUninitialize();
            m_failed = true;
        }
    }
    if( !m_failed )
    {
        hres = pLoc->ConnectServer( L"ROOT\\CIMV2", NULL, NULL, 0, NULL, 0, 0, &pSvc );
        if( FAILED(hres) )
        {
            qCritical() << "Could not connect. Error code = " << hres << endl;
            pLoc->Release();
            if ( m_bNeedUninit )
                CoUninitialize();
            m_failed = true;
        }
        //        else
        //            qDebug() << "Connected to ROOT\\CIMV2 WMI namespace" << endl;
    }

    if( !m_failed )
    {
        hres = CoSetProxyBlanket( pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                                  RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE );
        if( FAILED(hres) )
        {
            qCritical() << "Could not set proxy blanket. Error code = " << hres << endl;
            pSvc->Release();
            pLoc->Release();
            if ( m_bNeedUninit )
                CoUninitialize();
            m_failed = true;
        }
    }
}

WmiQuery::~WmiQuery()
{
    if( m_failed )
        return; // already cleaned up properly
    if( pSvc )
      pSvc->Release();
    if( pLoc )
      pLoc->Release();
    if( m_bNeedUninit )
      CoUninitialize();
}

void WmiQuery::addDeviceListeners(WmiManager::WmiEventSink *sink){
    if(m_failed)
        return;
    BSTR bstrQuery;
    bstrQuery = ::SysAllocString(sink->query());
    HRESULT hr = pSvc->ExecNotificationQueryAsync(L"WQL",bstrQuery,0, NULL,sink);
    ::SysFreeString(bstrQuery);
    if(FAILED(hr)){
        qWarning()<<"WmiQuery::addDeviceListeners "<<sink->query()<<" failed!";
    }
}

WmiQuery::ItemList WmiQuery::sendQuery( const QString &wql )
{
//    qDebug()<<"WmiQuery::ItemList WmiQuery::sendQuery"<<wql;
    ItemList retList;

    if (!pSvc)
    {
        m_failed = true;
        return retList;
    }

    HRESULT hres;

    IEnumWbemClassObject* pEnumerator = NULL;
    BSTR bstrQuery;
    bstrQuery = ::SysAllocString(wql);
    hres = pSvc->ExecQuery( L"WQL",bstrQuery ,
                            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
    ::SysFreeString(bstrQuery);
    
    if( FAILED(hres) )
    {
        qDebug() << "Query with string \"" << wql << "\" failed. Error code = " << hres << endl;
    }
    else
    {
        ULONG uReturn = 0;

        while( pEnumerator )
        {
            IWbemClassObject *pclsObj;
            hres = pEnumerator->Next( WBEM_INFINITE, 1, &pclsObj, &uReturn );

            if( !uReturn )
                break;

            // pclsObj will be released on destruction of Item
            retList.append( Item( pclsObj ) );
            pclsObj->Release();
        }
        if( pEnumerator )
            pEnumerator->Release();
        else
            qDebug() << "failed to release enumerator!";
    }
//        if(retList.size()== 0)
//            qDebug()<<"querying"<<wql<<"returned empty list";
//        else
//            qDebug()<<"Feteched"<<retList.size()<<"items";
    return retList;
}

bool WmiQuery::isLegit() const
{
    return !m_failed;
}

WmiQuery &WmiQuery::instance()
{
    static WmiQuery *query = 0;
    if (!query)
        query = new WmiQuery;
    return *query;
}
