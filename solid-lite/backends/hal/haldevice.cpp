/*
    Copyright 2005-2007 Kevin Ottens <ervin@kde.org>

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

#include "haldevice.h"

#include <QDebug>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusArgument>
#include <QDBusMetaType>
#include <QDBusVariant>

#include <solid-lite/genericinterface.h>

#include "haldeviceinterface.h"
#include "halgenericinterface.h"
//#include "halprocessor.h"
#include "halblock.h"
#include "halstorageaccess.h"
#include "halstorage.h"
//#include "halcdrom.h"
#include "halvolume.h"
//#include "halopticaldisc.h"
//#include "halcamera.h"
#include "halportablemediaplayer.h"
//#include "halnetworkinterface.h"
//#include "halacadapter.h"
//#include "halbattery.h"
//#include "halbutton.h"
//#include "halaudiointerface.h"
//#include "haldvbinterface.h"
//#include "halvideo.h"
//#include "halserialinterface.h"
//#include "halsmartcardreader.h"

using namespace Solid::Backends::Hal;

// Adapted from KLocale as Solid needs to be Qt-only
static QString formatByteSize(double size)
{
    // Per IEC 60027-2

    // Binary prefixes
    //Tebi-byte             TiB             2^40    1,099,511,627,776 bytes
    //Gibi-byte             GiB             2^30    1,073,741,824 bytes
    //Mebi-byte             MiB             2^20    1,048,576 bytes
    //Kibi-byte             KiB             2^10    1,024 bytes

    QString s;
    // Gibi-byte
    if ( size >= 1073741824.0 )
    {
        size /= 1073741824.0;
        if ( size > 1024 ) // Tebi-byte
            s = QObject::tr("%1 TiB").arg(QLocale().toString(size / 1024.0, 'f', 1));
        else
            s = QObject::tr("%1 GiB").arg(QLocale().toString(size, 'f', 1));
    }
    // Mebi-byte
    else if ( size >= 1048576.0 )
    {
        size /= 1048576.0;
        s = QObject::tr("%1 MiB").arg(QLocale().toString(size, 'f', 1));
    }
    // Kibi-byte
    else if ( size >= 1024.0 )
    {
        size /= 1024.0;
        s = QObject::tr("%1 KiB").arg(QLocale().toString(size, 'f', 1));
    }
    // Just byte
    else if ( size > 0 )
    {
        s = QObject::tr("%1 B").arg(QLocale().toString(size, 'f', 1));
    }
    // Nothing
    else
    {
        s = QObject::tr("0 B");
    }
    return s;
}

class Solid::Backends::Hal::HalDevicePrivate
{
public:
    HalDevicePrivate(const QString &udi)
        : device("org.freedesktop.Hal",
                  udi,
                  "org.freedesktop.Hal.Device",
                  QDBusConnection::systemBus()),
          cacheSynced(false), parent(0) { }
    void checkCache(const QString &key = QString());

    QDBusInterface device;
    QMap<QString,QVariant> cache;
    QMap<Solid::DeviceInterface::Type, bool> capListCache;
    QSet<QString> invalidKeys;

    bool cacheSynced;
    HalDevice *parent;
};

Q_DECLARE_METATYPE(ChangeDescription)
Q_DECLARE_METATYPE(QList<ChangeDescription>)

const QDBusArgument &operator<<(QDBusArgument &arg, const ChangeDescription &change)
{
    arg.beginStructure();
    arg << change.key << change.added << change.removed;
    arg.endStructure();
    return arg;
}

const QDBusArgument &operator>>(const QDBusArgument &arg, ChangeDescription &change)
{
    arg.beginStructure();
    arg >> change.key >> change.added >> change.removed;
    arg.endStructure();
    return arg;
}

HalDevice::HalDevice(const QString &udi)
    : Device(), d(new HalDevicePrivate(udi))
{
    qDBusRegisterMetaType<ChangeDescription>();
    qDBusRegisterMetaType< QList<ChangeDescription> >();

    d->device.connection().connect("org.freedesktop.Hal",
                                    udi, "org.freedesktop.Hal.Device",
                                    "PropertyModified",
                                    this, SLOT(slotPropertyModified(int,QList<ChangeDescription>)));
    d->device.connection().connect("org.freedesktop.Hal",
                                    udi, "org.freedesktop.Hal.Device",
                                    "Condition",
                                    this, SLOT(slotCondition(QString,QString)));
}

HalDevice::~HalDevice()
{
    delete d->parent;
    delete d;
}

QString HalDevice::udi() const
{
    return prop("info.udi").toString();
}

QString HalDevice::parentUdi() const
{
    return prop("info.parent").toString();
}

QString HalDevice::vendor() const
{
    const QString category = prop("info.category").toString();

    if (category == QLatin1String("battery")) {
        return prop("battery.vendor").toString();
    } else {
        return prop("info.vendor").toString();
    }
}

QString HalDevice::product() const
{
    return prop("info.product").toString();
}

QString HalDevice::icon() const
{
    QString category = prop("info.category").toString();

    if(parentUdi().isEmpty()) {

        QString formfactor = prop("system.formfactor").toString();
        if (formfactor=="laptop") {
            return "computer-laptop";
        } else {
            return "computer";
        }

    } else if (category=="storage" || category=="storage.cdrom") {

        if (prop("storage.drive_type").toString()=="floppy") {
            return "media-floppy";
        } /*else if (prop("storage.drive_type").toString()=="cdrom") {
            return "drive-optical";
        } */ else if (prop("storage.drive_type").toString()=="sd_mmc") {
            return "media-flash-sd-mmc";
        } else if (prop("storage.hotpluggable").toBool()) {
            if (prop("storage.bus").toString()=="usb") {
                if (prop("storage.no_partitions_hint").toBool()
                 || prop("storage.removable.media_size").toLongLong()<4000000000LL) {
                    return "drive-removable-media-usb-pendrive";
                } else {
                    return "drive-removable-media-usb";
                }
            }

            return "drive-removable-media";
        }

        return "drive-harddisk";

    } else if (category=="volume" || category=="volume.disc") {

        QStringList capabilities = prop("info.capabilities").toStringList();

        if (capabilities.contains("volume.disc")) {
            bool has_video = prop("volume.disc.is_vcd").toBool()
                          || prop("volume.disc.is_svcd").toBool()
                          || prop("volume.disc.is_videodvd").toBool();
            bool has_audio = prop("volume.disc.has_audio").toBool();
            bool recordable = prop("volume.disc.is_blank").toBool()
                          || prop("volume.disc.is_appendable").toBool()
                          || prop("volume.disc.is_rewritable").toBool();

            /*if (has_video) {
                return "media-optical-video";
            } else if (has_audio) {
                return "media-optical-audio";
            } else if (recordable) {
                return "media-optical-recordable";
            } else*/ {
                return "media-optical";
            }

        } else {
            if (!d->parent) {
                d->parent = new HalDevice(parentUdi());
            }
            QString iconName = d->parent->icon();

            if (!iconName.isEmpty()) {
                return iconName;
            }

            return "drive-harddisk";
        }

    } /*else if (category=="camera") {
        return "camera-photo";

    } else if (category=="input") {
        QStringList capabilities = prop("info.capabilities").toStringList();

        if (capabilities.contains("input.mouse")) {
            return "input-mouse";
        } else if (capabilities.contains("input.keyboard")) {
            return "input-keyboard";
        } else if (capabilities.contains("input.joystick")) {
            return "input-gaming";
        } else if (capabilities.contains("input.tablet")) {
            return "input-tablet";
        }

    } */ else if (category=="portable_audio_player") {
        QStringList protocols = prop("portable_audio_player.access_method.protocols").toStringList();

        if (protocols.contains("ipod")) {
            return "multimedia-player-apple-ipod";
        } else {
            return "multimedia-player";
        }
    } /* else if (category=="battery") {
        return "battery";
    } else if (category=="processor") {
        return "cpu"; // FIXME: Doesn't follow icon spec
    } else if (category=="video4linux") {
        return "camera-web";
    } else if (category == "alsa" || category == "oss") {
        // Sorry about this const_cast, but it's the best way to not copy the code from
        // AudioInterface.
        const Hal::AudioInterface audioIface(const_cast<HalDevice *>(this));
        switch (audioIface.soundcardType()) {
        case Solid::AudioInterface::InternalSoundcard:
            return QLatin1String("audio-card");
        case Solid::AudioInterface::UsbSoundcard:
            return QLatin1String("audio-card-usb");
        case Solid::AudioInterface::FirewireSoundcard:
            return QLatin1String("audio-card-firewire");
        case Solid::AudioInterface::Headset:
            if (udi().contains("usb", Qt::CaseInsensitive) ||
                    audioIface.name().contains("usb", Qt::CaseInsensitive)) {
                return QLatin1String("audio-headset-usb");
            } else {
                return QLatin1String("audio-headset");
            }
        case Solid::AudioInterface::Modem:
            return QLatin1String("modem");
        }
    } else if (category == "serial") {
        // TODO - a serial device can be a modem, or just
        // a COM port - need a new icon?
        return QLatin1String("modem");
    } else if (category == "smart_card_reader") {
        return QLatin1String("smart-card-reader");
    } */

    return QString();
}

QStringList HalDevice::emblems() const
{
    QStringList res;

    if (queryDeviceInterface(Solid::DeviceInterface::StorageAccess)) {
        bool isEncrypted = prop("volume.fsusage").toString()=="crypto";

        const Hal::StorageAccess accessIface(const_cast<HalDevice *>(this));
        if (accessIface.isAccessible()) {
            if (isEncrypted) {
                res << "emblem-encrypted-unlocked";
            } else {
                res << "emblem-mounted";
            }
        } else {
            if (isEncrypted) {
                res << "emblem-encrypted-locked";
            } else {
                res << "emblem-unmounted";
            }
        }
    }

    return res;
}

QString HalDevice::description() const
{
    QString category = prop("info.category").toString();

    if (category=="storage" || category=="storage.cdrom") {
        return storageDescription();
    } else if (category=="volume" || category=="volume.disc") {
        return volumeDescription();
    } /*else if (category=="net.80211") {
        return QObject::tr("WLAN Interface");
    } else if (category=="net.80203") {
        return QObject::tr("Networking Interface");
    } */ else {
        return product();
    }
}

QVariant HalDevice::prop(const QString &key) const
{
    d->checkCache(key);
    return d->cache.value(key);
}

void HalDevicePrivate::checkCache(const QString &key)
{
    if (cacheSynced) {
        if (key.isEmpty()) {
            if (invalidKeys.isEmpty()) {
                return;
            }
        } else if (!invalidKeys.contains(key)) {
            return;
        }
    }

    QDBusReply<QVariantMap> reply = device.call("GetAllProperties");

    if (reply.isValid()) {
        cache = reply;
    } else {
        qWarning() << Q_FUNC_INFO << " error: " << reply.error().name()
            << ", " << reply.error().message() << endl;
        cache = QVariantMap();
    }

    invalidKeys.clear();
    cacheSynced = true;
    //qDebug( )<< q << udi() << "failure";
}

QMap<QString, QVariant> HalDevice::allProperties() const
{
    d->checkCache();
    return d->cache;
}

bool HalDevice::propertyExists(const QString &key) const
{
    d->checkCache(key);
    return d->cache.value(key).isValid();
}

bool HalDevice::queryDeviceInterface(const Solid::DeviceInterface::Type &type) const
{
    // Special cases not matching with HAL capabilities
    if (type==Solid::DeviceInterface::GenericInterface) {
        return true;
    } else if (type==Solid::DeviceInterface::StorageAccess) {
        return prop("org.freedesktop.Hal.Device.Volume.method_names").toStringList().contains("Mount")
            || prop("info.interfaces").toStringList().contains("org.freedesktop.Hal.Device.Volume.Crypto");
    }
    /*else if (type==Solid::DeviceInterface::Video) {
        if (!prop("video4linux.device").toString().contains("video" ) )
          return false;
    } */else if (d->capListCache.contains(type)) {
        return d->capListCache.value(type);
    }

    QStringList cap_list = DeviceInterface::toStringList(type);

    foreach (const QString &cap, cap_list) {
        QDBusReply<bool> reply = d->device.call("QueryCapability", cap);

        if (!reply.isValid()) {
            qWarning() << Q_FUNC_INFO << " error: " << reply.error().name() << endl;
            return false;
        }

        if (reply) {
            d->capListCache.insert(type, true);
            return true;
        }
    }

    d->capListCache.insert(type, false);
    return false;
}

QObject *HalDevice::createDeviceInterface(const Solid::DeviceInterface::Type &type)
{
    if (!queryDeviceInterface(type)) {
        return 0;
    }

    DeviceInterface *iface = 0;

    switch (type)
    {
    case Solid::DeviceInterface::GenericInterface:
        iface = new GenericInterface(this);
        break;
    //case Solid::DeviceInterface::Processor:
    //    iface = new Processor(this);
    //    break;
    case Solid::DeviceInterface::Block:
        iface = new Block(this);
        break;
    case Solid::DeviceInterface::StorageAccess:
        iface = new StorageAccess(this);
        break;
    case Solid::DeviceInterface::StorageDrive:
        iface = new Storage(this);
        break;
    //case Solid::DeviceInterface::OpticalDrive:
    //    iface = new Cdrom(this);
    //    break;
    case Solid::DeviceInterface::StorageVolume:
        iface = new Volume(this);
        break;
    //case Solid::DeviceInterface::OpticalDisc:
    //    iface = new OpticalDisc(this);
    //    break;
    //case Solid::DeviceInterface::Camera:
    //    iface = new Camera(this);
    //    break;
    case Solid::DeviceInterface::PortableMediaPlayer:
        iface = new PortableMediaPlayer(this);
        break;
    /*case Solid::DeviceInterface::NetworkInterface:
        iface = new NetworkInterface(this);
        break;
    case Solid::DeviceInterface::AcAdapter:
        iface = new AcAdapter(this);
        break;
    case Solid::DeviceInterface::Battery:
        iface = new Battery(this);
        break;
    case Solid::DeviceInterface::Button:
        iface = new Button(this);
        break;
    case Solid::DeviceInterface::AudioInterface:
        iface = new AudioInterface(this);
        break;
    case Solid::DeviceInterface::DvbInterface:
        iface = new DvbInterface(this);
        break;
    case Solid::DeviceInterface::Video:
        iface = new Video(this);
        break;
    case Solid::DeviceInterface::SerialInterface:
        iface = new SerialInterface(this);
        break;
    case Solid::DeviceInterface::SmartCardReader:
        iface = new SmartCardReader(this);
        break;
    case Solid::DeviceInterface::InternetGateway:
        break;
    case Solid::DeviceInterface::NetworkShare:
        break;
    */
    case Solid::DeviceInterface::Unknown:
    case Solid::DeviceInterface::Last:
        break;
    }

    return iface;
}

void HalDevice::slotPropertyModified(int /*count */, const QList<ChangeDescription> &changes)
{
    QMap<QString,int> result;

    foreach (const ChangeDescription &change, changes) {
        QString key = change.key;
        bool added = change.added;
        bool removed = change.removed;

        Solid::GenericInterface::PropertyChange type = Solid::GenericInterface::PropertyModified;

        if (added) {
            type = Solid::GenericInterface::PropertyAdded;
        } else if (removed) {
            type = Solid::GenericInterface::PropertyRemoved;
        }

        result[key] = type;
        d->cache.remove(key);

        if (d->cache.isEmpty()) {
            d->cacheSynced = false;
            d->invalidKeys.clear();
        } else {
            d->invalidKeys.insert(key);
        }
    }

    //qDebug() << this << "unsyncing the cache";
    emit propertyChanged(result);
}

void HalDevice::slotCondition(const QString &condition, const QString &reason)
{
    emit conditionRaised(condition, reason);
}

QString HalDevice::storageDescription() const
{
    QString description;
    const Storage storageDrive(const_cast<HalDevice*>(this));
    Solid::StorageDrive::DriveType drive_type = storageDrive.driveType();
    bool drive_is_hotpluggable = storageDrive.isHotpluggable();

    /*if (drive_type == Solid::StorageDrive::CdromDrive) {
        const Cdrom opticalDrive(const_cast<HalDevice*>(this));
        Solid::OpticalDrive::MediumTypes mediumTypes = opticalDrive.supportedMedia();
        QString first;
        QString second;

        first = QObject::tr("CD-ROM", "First item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::Cdr)
            first = QObject::tr("CD-R", "First item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::Cdrw)
            first = QObject::tr("CD-RW", "First item of %1%2 Drive sentence");

        if (mediumTypes & Solid::OpticalDrive::Dvd)
            second = QObject::tr("/DVD-ROM", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::Dvdplusr)
            second = QObject::tr("/DVD+R", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::Dvdplusrw)
            second = QObject::tr("/DVD+RW", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::Dvdr)
            second = QObject::tr("/DVD-R", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::Dvdrw)
            second = QObject::tr("/DVD-RW", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::Dvdram)
            second = QObject::tr("/DVD-RAM", "Second item of %1%2 Drive sentence");
        if ((mediumTypes & Solid::OpticalDrive::Dvdr) && (mediumTypes & Solid::OpticalDrive::Dvdplusr)) {
            if(mediumTypes & Solid::OpticalDrive::Dvdplusdl)
                second = QObject::tr("/DVD±R DL", "Second item of %1%2 Drive sentence");
            else
                second = QObject::tr("/DVD±R", "Second item of %1%2 Drive sentence");
        }
        if ((mediumTypes & Solid::OpticalDrive::Dvdrw) && (mediumTypes & Solid::OpticalDrive::Dvdplusrw)) {
            if((mediumTypes & Solid::OpticalDrive::Dvdplusdl) || (mediumTypes & Solid::OpticalDrive::Dvdplusdlrw))
                second = QObject::tr("/DVD±RW DL", "Second item of %1%2 Drive sentence");
            else
                second = QObject::tr("/DVD±RW", "Second item of %1%2 Drive sentence");
        }
        if (mediumTypes & Solid::OpticalDrive::Bd)
            second = QObject::tr("/BD-ROM", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::Bdr)
            second = QObject::tr("/BD-R", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::Bdre)
            second = QObject::tr("/BD-RE", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::HdDvd)
            second = QObject::tr("/HD DVD-ROM", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::HdDvdr)
            second = QObject::tr("/HD DVD-R", "Second item of %1%2 Drive sentence");
        if (mediumTypes & Solid::OpticalDrive::HdDvdrw)
            second = QObject::tr("/HD DVD-RW", "Second item of %1%2 Drive sentence");

        if (drive_is_hotpluggable) {
            description = QObject::tr("External %1%2 Drive", "%1 is CD-ROM/CD-R/etc; %2 is '/DVD-ROM'/'/DVD-R'/etc (with leading slash)").arg(first).arg(second);
        } else {
            description = QObject::tr("%1%2 Drive", "%1 is CD-ROM/CD-R/etc; %2 is '/DVD-ROM'/'/DVD-R'/etc (with leading slash)").arg(first).arg(second);
        }

        return description;
    }*/

    /*if (drive_type == Solid::StorageDrive::Floppy) {
        if (drive_is_hotpluggable)
            description = QObject::tr("External Floppy Drive");
        else
            description = QObject::tr("Floppy Drive");

        return description;
    }*/

    bool drive_is_removable = storageDrive.isRemovable();

    if (drive_type == Solid::StorageDrive::HardDisk && !drive_is_removable) {
        QString size_str = formatByteSize(prop("storage.size").toInt());
        if (!size_str.isEmpty()) {
            if (drive_is_hotpluggable) {
                description = QObject::tr("%1 External Hard Drive", "%1 is the size").arg(size_str);
            } else {
                description = QObject::tr("%1 Hard Drive", "%1 is the size").arg(size_str);
            }
        } else {
            if (drive_is_hotpluggable)
                description = QObject::tr("External Hard Drive");
            else
                description = QObject::tr("Hard Drive");
        }

        return description;
    }

    QString vendormodel_str;
    QString model = prop("storage.model").toString();
    QString vendor = prop("storage.vendor").toString();

    if (vendor.isEmpty()) {
        if (!model.isEmpty())
            vendormodel_str = model;
    } else {
        if (model.isEmpty())
            vendormodel_str = vendor;
        else
            vendormodel_str = QObject::tr("%1 %2", "%1 is the vendor, %2 is the model of the device").arg(vendor).arg(model);
    }

    if (vendormodel_str.isEmpty())
        description = QObject::tr("Drive");
    else
        description = vendormodel_str;

    return description;
}

QString HalDevice::volumeDescription() const
{
    QString description;
    QString volume_label = prop("volume.label").toString();

    if (!volume_label.isEmpty()) {
        return volume_label;
    }

    if (!d->parent) {
        d->parent = new HalDevice(parentUdi());
    }
    const Storage storageDrive(const_cast<HalDevice*>(d->parent));
    Solid::StorageDrive::DriveType drive_type = storageDrive.driveType();

    /* Handle media in optical drives */
    /*if (drive_type == Solid::StorageDrive::CdromDrive) {
        const OpticalDisc disc(const_cast<HalDevice*>(this));
        switch (disc.discType()) {
            case Solid::OpticalDisc::UnknownDiscType:
            case Solid::OpticalDisc::CdRom:
                description = QObject::tr("CD-ROM");
                break;

            case Solid::OpticalDisc::CdRecordable:
                if (disc.isBlank())
                    description = QObject::tr("Blank CD-R");
                else
                    description = QObject::tr("CD-R");
                break;

            case Solid::OpticalDisc::CdRewritable:
                if (disc.isBlank())
                    description = QObject::tr("Blank CD-RW");
                else
                    description = QObject::tr("CD-RW");
                break;

            case Solid::OpticalDisc::DvdRom:
                description = QObject::tr("DVD-ROM");
                break;

            case Solid::OpticalDisc::DvdRam:
                if (disc.isBlank())
                    description = QObject::tr("Blank DVD-RAM");
                else
                    description = QObject::tr("DVD-RAM");
                break;

            case Solid::OpticalDisc::DvdRecordable:
                if (disc.isBlank())
                    description = QObject::tr("Blank DVD-R");
                else
                    description = QObject::tr("DVD-R");
                break;

            case Solid::OpticalDisc::DvdPlusRecordableDuallayer:
                if (disc.isBlank())
                    description = QObject::tr("Blank DVD+R Dual-Layer");
                else
                    description = QObject::tr("DVD+R Dual-Layer");
                break;

            case Solid::OpticalDisc::DvdRewritable:
                if (disc.isBlank())
                    description = QObject::tr("Blank DVD-RW");
                else
                    description = QObject::tr("DVD-RW");
                break;

            case Solid::OpticalDisc::DvdPlusRecordable:
                if (disc.isBlank())
                    description = QObject::tr("Blank DVD+R");
                else
                    description = QObject::tr("DVD+R");
                break;

            case Solid::OpticalDisc::DvdPlusRewritable:
                if (disc.isBlank())
                    description = QObject::tr("Blank DVD+RW");
                else
                    description = QObject::tr("DVD+RW");
                break;

            case Solid::OpticalDisc::DvdPlusRewritableDuallayer:
                if (disc.isBlank())
                    description = QObject::tr("Blank DVD+RW Dual-Layer");
                else
                    description = QObject::tr("DVD+RW Dual-Layer");
                break;

            case Solid::OpticalDisc::BluRayRom:
                description = QObject::tr("BD-ROM");
                break;

            case Solid::OpticalDisc::BluRayRecordable:
                if (disc.isBlank())
                    description = QObject::tr("Blank BD-R");
                else
                    description = QObject::tr("BD-R");
                break;

            case Solid::OpticalDisc::BluRayRewritable:
                if (disc.isBlank())
                    description = QObject::tr("Blank BD-RE");
                else
                    description = QObject::tr("BD-RE");
                break;

            case Solid::OpticalDisc::HdDvdRom:
                description = QObject::tr("HD DVD-ROM");
                break;

            case Solid::OpticalDisc::HdDvdRecordable:
                if (disc.isBlank())
                    description = QObject::tr("Blank HD DVD-R");
                else
                    description = QObject::tr("HD DVD-R");
                break;

            case Solid::OpticalDisc::HdDvdRewritable:
                if (disc.isBlank())
                    description = QObject::tr("Blank HD DVD-RW");
                else
                    description = QObject::tr("HD DVD-RW");
                break;
            }

        / * Special case for pure audio disc * /
        if (disc.availableContent() == Solid::OpticalDisc::Audio) {
            description = QObject::tr("Audio CD");
        }

        return description;
    }*/

    bool drive_is_removable = storageDrive.isRemovable();
    bool drive_is_hotpluggable = storageDrive.isHotpluggable();
    bool drive_is_encrypted_container = prop("volume.fsusage").toString()=="crypto";

    QString size_str = formatByteSize(prop("volume.size").toULongLong());
    if (drive_is_encrypted_container) {
        if (!size_str.isEmpty()) {
            description = QObject::tr("%1 Encrypted Container", "%1 is the size").arg(size_str);
        } else {
            description = QObject::tr("Encrypted Container");
        }
    } else if (drive_type == Solid::StorageDrive::HardDisk && !drive_is_removable) {
        if (!size_str.isEmpty()) {
            if (drive_is_hotpluggable) {
                description = QObject::tr("%1 External Hard Drive", "%1 is the size").arg(size_str);
            } else {
                description = QObject::tr("%1 Hard Drive", "%1 is the size").arg(size_str);
            }
        } else {
            if (drive_is_hotpluggable)
                description = QObject::tr("External Hard Drive");
            else
                description = QObject::tr("Hard Drive");
        }
    } else {
        if (drive_is_removable) {
            description = QObject::tr("%1 Removable Media", "%1 is the size").arg(size_str);
        } else {
            description = QObject::tr("%1 Media", "%1 is the size").arg(size_str);
        }
    }

    return description;
}

//#include "backends/hal/haldevice.moc"
