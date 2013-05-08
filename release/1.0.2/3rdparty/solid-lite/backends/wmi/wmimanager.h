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

#ifndef SOLID_BACKENDS_WMI_WMIMANAGER_H
#define SOLID_BACKENDS_WMI_WMIMANAGER_H

#include <solid-lite/ifaces/devicemanager.h>
#include <solid-lite/deviceinterface.h>

#include <QVariant>
#include <QStringList>

#include <Wbemidl.h>

namespace Solid
{
namespace Backends
{
namespace Wmi
{
class WmiManagerPrivate;

class WmiManager : public Solid::Ifaces::DeviceManager
{
    Q_OBJECT

public:

    class WmiEventSink : public IWbemObjectSink
    {
    public:
        WmiEventSink(class WmiManager* parent,const QString &query,const QList<Solid::DeviceInterface::Type> &types);
        ~WmiEventSink();

        virtual ulong STDMETHODCALLTYPE AddRef();
        virtual ulong STDMETHODCALLTYPE Release();

        virtual HRESULT  STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppv);

        virtual HRESULT STDMETHODCALLTYPE Indicate(long lObjectCount,IWbemClassObject **apObjArray);

        virtual HRESULT STDMETHODCALLTYPE SetStatus(long lFlags,HRESULT hResult,BSTR strParam,IWbemClassObject *pObjParam);

        const QString& query() const;

    private:
        WmiManager *m_parent;
        QString m_query;
        QList<Solid::DeviceInterface::Type> m_types;
        long m_count;

    };

    WmiManager(QObject *parent=0);
    virtual ~WmiManager();

    virtual QString udiPrefix() const ;
    virtual QSet<Solid::DeviceInterface::Type> supportedInterfaces() const;

    virtual QStringList allDevices();
    virtual bool deviceExists(const QString &udi);

    virtual QStringList devicesFromQuery(const QString &parentUdi,
                                         Solid::DeviceInterface::Type type);

    virtual QObject *createDevice(const QString &udi);


private Q_SLOTS:
    void slotDeviceAdded(const QString &udi);
    void slotDeviceRemoved(const QString &udi);

private:
    QStringList findDeviceStringMatch(const QString &key, const QString &value);
    QStringList findDeviceByDeviceInterface(Solid::DeviceInterface::Type type);

    WmiManagerPrivate *d;
    friend class WmiManagerPrivate;
};
}
}
}

#endif // SOLID_BACKENDS_WMI_WMIMANAGER_H
