/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/****************************************************************************************
 * Copyright (c) 2007 Jeff Mitchell <kde-dev@emailgoeshere.com>                         *
 *                                                                                      *
 * This program is free software; you can redistribute it and/or modify it under        *
 * the terms of the GNU General Public License as published by the Free Software        *
 * Foundation; either version 2 of the License, or (at your option) any later           *
 * version.                                                                             *
 *                                                                                      *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY      *
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A      *
 * PARTICULAR PURPOSE. See the GNU General Public License for more details.             *
 *                                                                                      *
 * You should have received a copy of the GNU General Public License along with         *
 * this program.  If not, see <http://www.gnu.org/licenses/>.                           *
 ****************************************************************************************/

#include "mediadevicecache.h"

#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <KDE/KConfig>
#include <solid/device.h>
#include <solid/deviceinterface.h>
#include <solid/devicenotifier.h>
#include <solid/genericinterface.h>
#include <solid/opticaldisc.h>
#include <solid/portablemediaplayer.h>
#include <solid/storageaccess.h>
#include <solid/storagedrive.h>
#include <solid/block.h>
#include <solid/storagevolume.h>

#include <kdeversion.h>

#include <kmountpoint.h>

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QList>

#define DEBUG_BLOCK
#include <QtCore/QDebug>
#define debug qDebug

MediaDeviceCache* MediaDeviceCache::s_instance = 0;

MediaDeviceCache::MediaDeviceCache() : QObject()
                             , m_type()
                             , m_name()
                             , m_volumes()
{
    DEBUG_BLOCK
    s_instance = this;
    connect( Solid::DeviceNotifier::instance(), SIGNAL( deviceAdded( const QString & ) ),
             this, SLOT( slotAddSolidDevice( const QString & ) ) );
    connect( Solid::DeviceNotifier::instance(), SIGNAL( deviceRemoved( const QString & ) ),
             this, SLOT( slotRemoveSolidDevice( const QString & ) ) );
//     connect(&m_timer, SIGNAL(timeout()), this, SLOT(slotTimeout()));

//     m_timer.setSingleShot(true);
//     m_timer.start(1000);
}

MediaDeviceCache::~MediaDeviceCache()
{
    s_instance = 0;
}

void
MediaDeviceCache::refreshCache()
{
    DEBUG_BLOCK
    m_type.clear();
    m_name.clear();
    QList<Solid::Device> deviceList = Solid::Device::listFromType( Solid::DeviceInterface::PortableMediaPlayer );
    foreach( const Solid::Device &device, deviceList )
    {
        if( device.as<Solid::StorageDrive>() )
        {
            debug() << "Found Solid PMP that is also a StorageDrive, skipping";
            continue;
        }
        debug() << "Found Solid::DeviceInterface::PortableMediaPlayer with udi = " << device.udi();
        debug() << "Device name is = " << device.product() << " and was made by " << device.vendor();
        m_type[device.udi()] = MediaDeviceCache::SolidPMPType;
        m_name[device.udi()] = device.vendor() + " - " + device.product();
    }
    deviceList = Solid::Device::listFromType( Solid::DeviceInterface::StorageAccess );
    foreach( const Solid::Device &device, deviceList )
    {
        debug() << "Found Solid::DeviceInterface::StorageAccess with udi = " << device.udi();
        debug() << "Device name is = " << device.product() << " and was made by " << device.vendor();

        const Solid::StorageAccess* ssa = device.as<Solid::StorageAccess>();

        if( ssa )
        {
            if( (!device.parent().as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.parent().as<Solid::StorageDrive>()->bus()) &&
                (!device.as<Solid::StorageDrive>() || Solid::StorageDrive::Usb!=device.as<Solid::StorageDrive>()->bus()) )
            {
                debug() << "Found Solid::DeviceInterface::StorageAccess that is not usb, skipping";
                continue;
            }
            if( !m_volumes.contains( device.udi() ) )
            {
                connect( ssa, SIGNAL( accessibilityChanged(bool, const QString&) ),
                    this, SLOT( slotAccessibilityChanged(bool, const QString&) ) );
                m_volumes.append( device.udi() );
            }
            if( ssa->isAccessible() )
            {
                m_type[device.udi()] = MediaDeviceCache::SolidVolumeType;
                m_name[device.udi()] = ssa->filePath();
                m_accessibility[ device.udi() ] = true;
            }
            else
            {
                m_accessibility[ device.udi() ] = false;
                debug() << "Solid device is not accessible, will wait until it is to consider it added.";
            }
        }
    }
//     deviceList = Solid::Device::listFromType( Solid::DeviceInterface::StorageDrive );
//     foreach( const Solid::Device &device, deviceList )
//     {
//         debug() << "Found Solid::DeviceInterface::StorageDrive with udi = " << device.udi();
//         debug() << "Device name is = " << device.product() << " and was made by " << device.vendor();
//
//         if( device.as<Solid::StorageDrive>() )
//         {
//             m_type[device.udi()] = MediaDeviceCache::SolidGenericType;
//             m_name[device.udi()] = device.vendor() + " - " + device.product();
//         }
//     }
//     deviceList = Solid::Device::listFromType( Solid::DeviceInterface::OpticalDisc );
//     foreach( const Solid::Device &device, deviceList )
//     {
//         debug() << "Found Solid::DeviceInterface::OpticalDisc with udi = " << device.udi();
//         debug() << "Device name is = " << device.product() << " and was made by " << device.vendor();
//
//         const Solid::OpticalDisc * opt = device.as<Solid::OpticalDisc>();
//
//         if ( opt && opt->availableContent() & Solid::OpticalDisc::Audio )
//         {
//             debug() << "device is an Audio CD";
//             m_type[device.udi()] = MediaDeviceCache::SolidAudioCdType;
//             m_name[device.udi()] = device.vendor() + " - " + device.product();
//         }
//     }
//     deviceList = Solid::Device::allDevices();
//     foreach( const Solid::Device &device, deviceList )
//     {
//         if( const Solid::GenericInterface *generic = device.as<Solid::GenericInterface>() )
//         {
//             if( m_type.contains( device.udi() ) )
//                 continue;
//
//             const QMap<QString, QVariant> properties = generic->allProperties();
//             if( !properties.contains("info.capabilities") )
//                 continue;
//
//             const QStringList capabilities = properties["info.capabilities"].toStringList();
//             if( !capabilities.contains("afc") )
//                 continue;
//
//             debug() << "Found AFC capable Solid::DeviceInterface::GenericInterface with udi = " << device.udi();
//             debug() << "Device name is = " << device.product() << " and was made by " << device.vendor();
//
//             m_type[device.udi()] = MediaDeviceCache::SolidGenericType;
//             m_name[device.udi()] = device.vendor() + " - " + device.product();
//         }
//     }
//     KConfigGroup config(KGlobal::config(), "PortableDevices" );
//     const QStringList manualDeviceKeys = config.entryMap().keys();
//     foreach( const QString &udi, manualDeviceKeys )
//     {
//         if( udi.startsWith( "manual" ) )
//         {
//             debug() << "Found manual device with udi = " << udi;
//             m_type[udi] = MediaDeviceCache::ManualType;
//             m_name[udi] = udi.split( '|' )[1];
//         }
//     }
}

void
MediaDeviceCache::slotAddSolidDevice( const QString &udi )
{
    DEBUG_BLOCK
    Solid::Device device( udi );
    debug() << "Found new Solid device with udi = " << device.udi();
    debug() << "Device name is = " << device.product() << " and was made by " << device.vendor();
    Solid::StorageAccess *ssa = device.as<Solid::StorageAccess>();

    Solid::OpticalDisc * opt = device.as<Solid::OpticalDisc>();

    if ( opt && opt->availableContent() & Solid::OpticalDisc::Audio )
    {
        debug() << "device is an Audio CD";
        m_type[udi] = MediaDeviceCache::SolidAudioCdType;
        m_name[udi] = device.vendor() + " - " + device.product();
    }
    else if( ssa )
    {
        debug() << "volume is generic storage";
        if( !m_volumes.contains( device.udi() ) )
        {
            connect( ssa, SIGNAL( accessibilityChanged(bool, const QString&) ),
                this, SLOT( slotAccessibilityChanged(bool, const QString&) ) );
            m_volumes.append( device.udi() );
        }
        if( ssa->isAccessible() )
        {
            m_type[udi] = MediaDeviceCache::SolidVolumeType;
            m_name[udi] = ssa->filePath();
        }
        else
        {
            debug() << "storage volume is not accessible right now, not adding.";
            return;
        }
    }
    else if( device.is<Solid::StorageDrive>() )
    {
        debug() << "device is a Storage drive, still need a volume";
        m_type[udi] = MediaDeviceCache::SolidGenericType;
        m_name[udi] = device.vendor() + " - " + device.product();
    }
    else if( device.is<Solid::PortableMediaPlayer>() )
    {
        debug() << "device is a PMP";
        m_type[udi] = MediaDeviceCache::SolidPMPType;
        m_name[udi] = device.vendor() + " - " + device.product();
    }
    else if( const Solid::GenericInterface *generic = device.as<Solid::GenericInterface>() )
    {
        const QMap<QString, QVariant> properties = generic->allProperties();
        /* At least iPod touch 3G and iPhone 3G do not advertise AFC (Apple File
         * Connection) capabilities. Therefore we have to white-list them so that they are
         * still recognised ad iPods
         *
         * @see IpodConnectionAssistant::identify() for a quirk that is currently also
         * needed for proper identification of iPhone-like devices.
         */
        if ( !device.product().contains("iPod") && !device.product().contains("iPhone"))
        {
            if( !properties.contains("info.capabilities") )
            {
                debug() << "udi " << udi << " does not describe a portable media player or storage volume";
                return;
            }

            const QStringList capabilities = properties["info.capabilities"].toStringList();
            if( !capabilities.contains("afc") )
            {
                debug() << "udi " << udi << " does not describe a portable media player or storage volume";
                return;
            }
        }

        debug() << "udi" << udi << "is AFC cabable (Apple mobile device)";
        m_type[udi] = MediaDeviceCache::SolidGenericType;
        m_name[udi] = device.vendor() + " - " + device.product();
    }
    else
    {
        debug() << "udi " << udi << " does not describe a portable media player or storage volume";
        return;
    }
    emit deviceAdded( udi );
}

void
MediaDeviceCache::slotRemoveSolidDevice( const QString &udi )
{
    DEBUG_BLOCK
    debug() << "udi is: " << udi;
    Solid::Device device( udi );
    if( m_volumes.contains( udi ) )
    {
        disconnect( device.as<Solid::StorageAccess>(), SIGNAL( accessibilityChanged(bool, const QString&) ),
                    this, SLOT( slotAccessibilityChanged(bool, const QString&) ) );
        m_volumes.removeAll( udi );
        emit deviceRemoved( udi );
    }
    if( m_type.contains( udi ) )
    {
        m_type.remove( udi );
        m_name.remove( udi );
        emit deviceRemoved( udi );
        return;
    }
    debug() << "Odd, got a deviceRemoved at udi " << udi << " but it did not seem to exist in the first place...";
    emit deviceRemoved( udi );
}

// void MediaDeviceCache::slotTimeout()
// {
//     KMountPoint::List possibleMountList = KMountPoint::possibleMountPoints();
//     KMountPoint::List currentMountList = KMountPoint::currentMountPoints();
//     QList<Solid::Device> deviceList = Solid::Device::listFromType( Solid::DeviceInterface::StorageAccess );
//
//     for (KMountPoint::List::iterator it = possibleMountList.begin(); it != possibleMountList.end(); ++it) {
//         if ((*it)->mountType() == "nfs" || (*it)->mountType() == "nfs4" ||
//             (*it)->mountType() == "smb" || (*it)->mountType() == "cifs") {
//             QString path = (*it)->mountPoint();
//             bool mounted = false;
//             QString udi = QString();
//
//             foreach( const Solid::Device &device, deviceList )
//             {
//                 const Solid::StorageAccess* ssa = device.as<Solid::StorageAccess>();
//                 if( ssa && path == ssa->filePath())
//                     udi = device.udi();
//             }
//
//             for (KMountPoint::List::iterator it2 = currentMountList.begin(); it2 != currentMountList.end(); ++it2) {
//                 if ( (*it)->mountType() == (*it2)->mountType() &&
//                      (*it)->mountPoint() == (*it2)->mountPoint() ) {
//                     mounted = true;
//                     break;
//                 }
//             }
//
//             if ( m_accessibility[udi] != mounted ) {
//                 m_accessibility[udi] = mounted;
//                 slotAccessibilityChanged( mounted, udi);
//             }
//         }
//     }
//
//     m_timer.setSingleShot(true);
//     m_timer.start(1000);
// }

void
MediaDeviceCache::slotAccessibilityChanged( bool accessible, const QString &udi )
{
    debug() << "accessibility of device " << udi << " has changed to accessible = " << (accessible ? "true":"false");
    if( accessible )
    {
        Solid::Device device( udi );
        m_type[udi] = MediaDeviceCache::SolidVolumeType;
        Solid::StorageAccess *ssa = device.as<Solid::StorageAccess>();
        if( ssa )
            m_name[udi] = ssa->filePath();
        emit deviceAdded( udi );
        return;
    }
    else
    {
        if( m_type.contains( udi ) )
        {
            m_type.remove( udi );
            m_name.remove( udi );
            emit deviceRemoved( udi );
            return;
        }
        debug() << "Got accessibility changed to false but was not there in the first place...";
    }

    emit accessibilityChanged( accessible, udi );
}

MediaDeviceCache::DeviceType
MediaDeviceCache::deviceType( const QString &udi ) const
{
    if( m_type.contains( udi ) )
    {
        return m_type[udi];
    }
    return MediaDeviceCache::InvalidType;
}

const QString
MediaDeviceCache::deviceName( const QString &udi ) const
{
    if( m_name.contains( udi ) )
    {
        return m_name[udi];
    }
    return "ERR_NO_NAME"; //Should never happen!
}

const QString
MediaDeviceCache::device( const QString &udi ) const
{
    DEBUG_BLOCK
    Solid::Device device( udi );
    Solid::Device parent( device.parent() );
    if( !parent.isValid() )
    {
        debug() << udi << "has no parent, returning null string.";
        return QString();
    }

    Solid::Block* sb = parent.as<Solid::Block>();
    if( !sb  )
    {
        debug() << parent.udi() << "failed to convert to Block, returning null string.";
        return QString();
    }

    return sb->device();
}

bool
MediaDeviceCache::isGenericEnabled( const QString &udi ) const
{
    DEBUG_BLOCK
    if( m_type[udi] != MediaDeviceCache::SolidVolumeType )
    {
        debug() << "Not SolidVolumeType, returning false";
        return false;
    }
    Solid::Device device( udi );
    Solid::StorageAccess* ssa = device.as<Solid::StorageAccess>();
    if( !ssa || !ssa->isAccessible() )
    {
        debug() << "Not able to convert to StorageAccess or not accessible, returning false";
        return false;
    }
    if( device.parent().as<Solid::PortableMediaPlayer>() )
    {
        debug() << "Could convert parent to PortableMediaPlayer, returning true";
        return true;
    }
    if( QFile::exists( ssa->filePath() + QDir::separator() + ".is_audio_player" ) )
    {
        return true;
    }
    return false;
}

const QString
MediaDeviceCache::volumeMountPoint( const QString &udi ) const
{
    DEBUG_BLOCK
    Solid::Device device( udi );
    Solid::StorageAccess* ssa = device.as<Solid::StorageAccess>();
    if( !ssa || !ssa->isAccessible() )
    {
        debug() << "Not able to convert to StorageAccess or not accessible, returning empty";
        return QString();
    }
    return ssa->filePath();
}

