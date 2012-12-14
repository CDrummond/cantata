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

#ifndef SOLID_BACKENDS_WMI_WMIQUERY_H
#define SOLID_BACKENDS_WMI_WMIQUERY_H

#include <QtCore/QDebug>
#include <QtCore/QVariant>
#include <QtCore/QList>
#include <QtCore/QAtomicInt>
#include <QtCore/QSharedPointer>

#include <solid-lite/solid_export.h>


#include <windows.h>
#include <rpc.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <WTypes.h>

#include "wmimanager.h"

namespace Solid
{
namespace Backends
{
namespace Wmi
{
class WmiQuery
{
public:
    class Item {
    public:
        Item();
        Item(IWbemClassObject *p);
        Item(const Item& other);
        Item& operator=(const Item& other);
        ~Item();

        IWbemClassObject* data() const;
        bool isNull() const;
        QVariant getProperty(const QString &property) const;
        QVariantMap getAllProperties();

    private:

        static QVariant msVariantToQVariant(VARIANT msVariant, CIMTYPE variantType);
        QVariant getProperty(BSTR property) const;
        // QSharedPointer alone doesn't help because we need to call Release()
        IWbemClassObject* m_p;
        QVariantMap m_properies;
    };

    typedef QList<Item> ItemList;

    WmiQuery();
    ~WmiQuery();
    ItemList sendQuery( const QString &wql );
    void addDeviceListeners(WmiManager::WmiEventSink *sink);
    bool isLegit() const;
	static WmiQuery &instance();

private:
    bool m_failed;
    bool m_bNeedUninit;
    IWbemLocator *pLoc;
    IWbemServices *pSvc;
};
}
}
}

#endif
