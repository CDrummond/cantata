/*
    Copyright 2012 Patrick von Reth <vonreth@kde.org>
    Copyright 2005,2006 Kevin Ottens <ervin@kde.org>

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

#include "wmimanager.h"

#include <QDebug>

#include "wmidevice.h"
#include "wmideviceinterface.h"
#include "wmiquery.h"


using namespace Solid::Backends::Wmi;

class Solid::Backends::Wmi::WmiManagerPrivate
{
public:
    WmiManagerPrivate(WmiManager *parent)
        :m_parent(parent)
    {
        supportedInterfaces << Solid::DeviceInterface::GenericInterface
                            << Solid::DeviceInterface::Processor
                               //                           << Solid::DeviceInterface::Block
                            << Solid::DeviceInterface::StorageAccess
                            << Solid::DeviceInterface::StorageDrive
                            << Solid::DeviceInterface::OpticalDrive
                            << Solid::DeviceInterface::StorageVolume
                            << Solid::DeviceInterface::OpticalDisc
                               //                           << Solid::DeviceInterface::Camera
                               //                           << Solid::DeviceInterface::PortableMediaPlayer
                               //                           << Solid::DeviceInterface::NetworkInterface
                            << Solid::DeviceInterface::AcAdapter
                            << Solid::DeviceInterface::Battery
                               //                           << Solid::DeviceInterface::Button
                               //                           << Solid::DeviceInterface::AudioInterface
                               //                           << Solid::DeviceInterface::DvbInterface
                               //                           << Solid::DeviceInterface::Video
                               //                           << Solid::DeviceInterface::SerialInterface
                               //                           << Solid::DeviceInterface::SmartCardReader
                               ;

        update();
    }

    ~WmiManagerPrivate()
    {

    }

    void update(){
        init();

    }

    void init(){
        if(m_deviceCache.isEmpty())
        {
            foreach(const Solid::DeviceInterface::Type &dev, supportedInterfaces){
                updateDeviceCache(dev);
            }
        }
    }



    void updateDeviceCache(const Solid::DeviceInterface::Type & type){
        QSet<QString> devSet = m_parent->findDeviceByDeviceInterface(type).toSet();
        if(m_deviceCache.contains(type)){
            QSet<QString> added = devSet - m_deviceCache[type];
            foreach(const QString & s,added){
                m_parent->slotDeviceAdded(s);
            }
            QSet<QString> removed = m_deviceCache[type] - devSet;
            foreach(const QString & s,removed){
                m_parent->slotDeviceRemoved(s);
            }
        }
        m_deviceCache[type] = devSet;
    }


    WmiQuery::ItemList sendQuery( const QString &wql )
    {
        return WmiQuery::instance().sendQuery( wql );
    }

    WmiManager *m_parent;
    QSet<Solid::DeviceInterface::Type> supportedInterfaces;
    QMap<Solid::DeviceInterface::Type,QSet<QString> > m_deviceCache;
};


WmiManager::WmiManager(QObject *parent)
    : DeviceManager(parent)
{
    d = new WmiManagerPrivate(this);


    QList<Solid::DeviceInterface::Type> types;
    types<<Solid::DeviceInterface::StorageDrive<<Solid::DeviceInterface::StorageVolume;
    //partition added
    WmiQuery::instance().addDeviceListeners(new WmiManager::WmiEventSink(this,"SELECT * FROM __InstanceCreationEvent WITHIN 10 WHERE TargetInstance ISA 'Win32_DiskPartition'",types));
    //partition removed
    WmiQuery::instance().addDeviceListeners(new WmiManager::WmiEventSink(this,"SELECT * FROM __InstanceDeletionEvent WITHIN 10 WHERE TargetInstance ISA 'Win32_DiskPartition'",types));

    types.clear();
    types<<Solid::DeviceInterface::OpticalDisc;
    //MediaLoaded=True/False change
    WmiQuery::instance().addDeviceListeners(new WmiManager::WmiEventSink(this,"SELECT * from __InstanceModificationEvent WITHIN 10 WHERE TargetInstance ISA 'Win32_CDromDrive'",types));

}

WmiManager::~WmiManager()
{
    delete d;
}

QString WmiManager::udiPrefix() const
{
    return QString(); //FIXME: We should probably use a prefix there... has to be tested on Windows
}

QSet<Solid::DeviceInterface::Type> WmiManager::supportedInterfaces() const
{
    return d->supportedInterfaces;
}

QStringList WmiManager::allDevices()
{
    QStringList list;
    foreach(const Solid::DeviceInterface::Type &type,d->supportedInterfaces){
        list<<d->m_deviceCache[type].toList();
    }
    return list;
}

bool WmiManager::deviceExists(const QString &udi)
{
    return WmiDevice::exists(udi);
}


QStringList WmiManager::devicesFromQuery(const QString &parentUdi,
                                         Solid::DeviceInterface::Type type)
{
    QStringList result;
    if (!parentUdi.isEmpty())
    {
        foreach(const QString &udi,allDevices()){
            WmiDevice device(udi);
            if(device.type() == type && device.parentUdi() == parentUdi ){
                result<<udi;
            }
        }

    } else if (type!=Solid::DeviceInterface::Unknown) {
            result<<findDeviceByDeviceInterface(type);
        } else {
            result<<allDevices();
        }
    return result;
}

QObject *WmiManager::createDevice(const QString &udi)
{
    if (deviceExists(udi)) {
        return new WmiDevice(udi);
    } else {
        return 0;
    }
}

QStringList WmiManager::findDeviceStringMatch(const QString &key, const QString &value)
{
    qDebug() << "has to be implemented" << key << value;
    QStringList result;

//    qDebug() << result;
    return result;
}

QStringList WmiManager::findDeviceByDeviceInterface(Solid::DeviceInterface::Type type)
{
    return  WmiDevice::generateUDIList(type);
}

void WmiManager::slotDeviceAdded(const QString &udi)
{
    qDebug()<<"Device added"<<udi;
    emit deviceAdded(udi);
}

void WmiManager::slotDeviceRemoved(const QString &udi)
{
    qDebug()<<"Device removed"<<udi;
    emit deviceRemoved(udi);
}



WmiManager::WmiEventSink::WmiEventSink(WmiManager* parent, const QString &query, const QList<Solid::DeviceInterface::Type> &types):
    m_parent(parent),
    m_query(query),
    m_types(types),
    m_count(0)
{}

WmiManager::WmiEventSink::~WmiEventSink()
{}

ulong STDMETHODCALLTYPE WmiManager::WmiEventSink::AddRef()
{
    return InterlockedIncrement(&m_count);
}

ulong STDMETHODCALLTYPE WmiManager::WmiEventSink::Release()
{
    long lRef = InterlockedDecrement(&m_count);
    if(lRef == 0)
        delete this;
    return lRef;
}

HRESULT  STDMETHODCALLTYPE WmiManager::WmiEventSink::QueryInterface(REFIID riid, void** ppv)
{
    if (riid == IID_IUnknown || riid == IID_IWbemObjectSink)
    {
        *ppv = (IWbemObjectSink *) this;
        AddRef();
        return WBEM_S_NO_ERROR;
    }
    else return E_NOINTERFACE;
}

HRESULT STDMETHODCALLTYPE WmiManager::WmiEventSink::Indicate(long lObjectCount,IWbemClassObject **apObjArray)
{
    foreach(const Solid::DeviceInterface::Type &type,m_types){
        m_parent->d->updateDeviceCache(type);
    }
    return WBEM_S_NO_ERROR;
}

HRESULT STDMETHODCALLTYPE WmiManager::WmiEventSink::SetStatus(long lFlags,HRESULT hResult,BSTR strParam,IWbemClassObject *pObjParam)
{
    return WBEM_S_NO_ERROR;
}

const QString&  WmiManager::WmiEventSink::query() const {
    return m_query;
}


//#include "backends/wmi/wmimanager.moc"
