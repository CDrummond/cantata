/*
    Copyright 2006 Kevin Ottens <ervin@kde.org>

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

#include "halstorageaccess.h"

#include "halfstabhandling.h"
#include "../../genericinterface.h"

#include <QLocale>
#include <QDebug>
#include <QProcess>
#include <QTimer>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDBusVariant>
#include <QApplication>
#include <QWidget>

#include <unistd.h>
#include <stdlib.h>

#ifdef Q_OS_FREEBSD
#include <langinfo.h>
#endif

using namespace Solid::Backends::Hal;

StorageAccess::StorageAccess(HalDevice *device)
    : DeviceInterface(device), m_setupInProgress(false), m_teardownInProgress(false), m_ejectInProgress(false),
      m_passphraseRequested(false)
{
    connect(device, SIGNAL(propertyChanged(QMap<QString,int>)),
             this, SLOT(slotPropertyChanged(QMap<QString,int>)));
    // Delay connecting to DBus signals to avoid the related time penalty
    // in hot paths such as predicate matching
    QTimer::singleShot(0, this, SLOT(connectDBusSignals()));
}

StorageAccess::~StorageAccess()
{

}

void StorageAccess::connectDBusSignals()
{
    m_device->registerAction("setup", this,
                             SLOT(slotSetupRequested()),
                             SLOT(slotSetupDone(int,QString)));

    m_device->registerAction("teardown", this,
                             SLOT(slotTeardownRequested()),
                             SLOT(slotTeardownDone(int,QString)));

    m_device->registerAction("eject", this,
                             SLOT(slotEjectRequested()),
                             SLOT(slotEjectDone(int,QString)));
}

void StorageAccess::slotSetupDone(int error, const QString &errorString)
{
    m_setupInProgress = false;
    emit setupDone(static_cast<Solid::ErrorType>(error), errorString, m_device->udi());
}

void StorageAccess::slotTeardownDone(int error, const QString &errorString)
{
    m_teardownInProgress = false;
    emit teardownDone(static_cast<Solid::ErrorType>(error), errorString, m_device->udi());
}

void StorageAccess::slotEjectDone(int error, const QString &errorString)
{
    m_ejectInProgress = false;
    emit ejectDone(static_cast<Solid::ErrorType>(error), errorString, m_device->udi());
}

bool StorageAccess::isAccessible() const
{
    if (m_device->prop("info.interfaces").toStringList().contains("org.freedesktop.Hal.Device.Volume.Crypto")) {

        // Might be a bit slow, but I see no cleaner way to do this with HAL...
        QDBusInterface manager("org.freedesktop.Hal",
                               "/org/freedesktop/Hal/Manager",
                               "org.freedesktop.Hal.Manager",
                               QDBusConnection::systemBus());

        QDBusReply<QStringList> reply = manager.call("FindDeviceStringMatch",
                                                     "volume.crypto_luks.clear.backing_volume",
                                                     m_device->udi());

        QStringList list = reply;

        return reply.isValid() && !list.isEmpty();

    } else {
        return m_device->prop("volume.is_mounted").toBool();
    }
}

QString StorageAccess::filePath() const
{
    QString result = m_device->prop("volume.mount_point").toString();

    if (result.isEmpty()) {
        QStringList mountpoints
            = FstabHandling::possibleMountPoints(m_device->prop("block.device").toString());
        if (mountpoints.size()==1) {
            result = mountpoints.first();
        }
    }

    return result;
}

bool StorageAccess::isIgnored() const
{
    HalDevice lock("/org/freedesktop/Hal/devices/computer");
    bool isLocked = lock.prop("info.named_locks.Global.org.freedesktop.Hal.Device.Storage.locked").toBool();

    if (m_device->prop("volume.ignore").toBool() || isLocked ){
        return true;
    }

    const QString mount_point = StorageAccess(m_device).filePath();
    const bool mounted = m_device->prop("volume.is_mounted").toBool();
    if (!mounted) {
        return false;
    } else if (mount_point.startsWith(QLatin1String("/media/")) || mount_point.startsWith(QLatin1String("/mnt/"))) {
        return false;
    }

    /* Now be a bit more aggressive on what we want to ignore,
     * the user generally need to check only what's removable or in /media
     * the volumes mounted to make the system (/, /boot, /var, etc.)
     * are useless to him.
     */
    Solid::Device drive(m_device->prop("block.storage_device").toString());

    const bool removable = drive.as<Solid::GenericInterface>()->property("storage.removable").toBool();
    const bool hotpluggable = drive.as<Solid::GenericInterface>()->property("storage.hotpluggable").toBool();

    return !removable && !hotpluggable;
}

bool StorageAccess::setup()
{
    if (m_teardownInProgress || m_setupInProgress || isAccessible()) {
        return false;
    }
    m_setupInProgress = true;
    m_device->broadcastActionRequested("setup");

    if (m_device->prop("info.interfaces").toStringList().contains("org.freedesktop.Hal.Device.Volume.Crypto")) {
        return requestPassphrase();
    } else if (FstabHandling::isInFstab(m_device->prop("block.device").toString())) {
        return callSystemMount();
    } else {
        return callHalVolumeMount();
    }
}

bool StorageAccess::teardown()
{
    if (m_teardownInProgress || m_setupInProgress || !isAccessible()) {
        return false;
    }
    m_teardownInProgress = true;
    m_device->broadcastActionRequested("teardown");

    if (m_device->prop("info.interfaces").toStringList().contains("org.freedesktop.Hal.Device.Volume.Crypto")) {
        return callCryptoTeardown();
    } else if (FstabHandling::isInFstab(m_device->prop("block.device").toString())) {
        return callSystemUnmount();
    } else {
        return callHalVolumeUnmount();
    }
}

void StorageAccess::slotPropertyChanged(const QMap<QString,int> &changes)
{
    if (changes.contains("volume.is_mounted"))
    {
        emit accessibilityChanged(isAccessible(), m_device->udi());
    }
}

void StorageAccess::slotDBusReply(const QDBusMessage &/*reply*/)
{
    if (m_setupInProgress) {
        m_setupInProgress = false;
        m_device->broadcastActionDone("setup");
    } else if (m_teardownInProgress) {
        m_teardownInProgress = false;
        m_device->broadcastActionDone("teardown");

        HalDevice drive(m_device->prop("block.storage_device").toString());
        if (drive.prop("storage.drive_type").toString()!="cdrom"
         && drive.prop("storage.requires_eject").toBool()) {

            QString devnode = m_device->prop("block.device").toString();

#if defined(Q_OS_OPENBSD)
            QString program = "cdio";
            QStringList args;
            args << "-f" << devnode << "eject";
#elif defined(Q_OS_FREEBSD) || defined(Q_OS_NETBSD)
            devnode.remove("/dev/").replace("([0-9]).", "\\1");
            QString program = "cdcontrol";
            QStringList args;
            args << "-f" << devnode << "eject";
#else
            QString program = "eject";
            QStringList args;
            args << devnode;
#endif

            m_ejectInProgress = true;
            m_device->broadcastActionRequested("eject");
            m_process = FstabHandling::callSystemCommand("eject", args,
                                                         this, SLOT(slotProcessFinished(int,QProcess::ExitStatus)));
        }
    } else if (m_ejectInProgress) {
        m_ejectInProgress = false;
        m_device->broadcastActionDone("eject");
    }
}

void StorageAccess::slotDBusError(const QDBusError &error)
{
    // TODO: Better error reporting here
    if (m_setupInProgress) {
        m_setupInProgress = false;
        m_device->broadcastActionDone("setup", Solid::UnauthorizedOperation,
                                      QString(error.name()+": "+error.message()));
    } else if (m_teardownInProgress) {
        m_teardownInProgress = false;
        m_device->broadcastActionDone("teardown", Solid::UnauthorizedOperation,
                                      QString(error.name()+": "+error.message()));
    } else if (m_ejectInProgress) {
        m_ejectInProgress = false;
        m_device->broadcastActionDone("eject", Solid::UnauthorizedOperation,
                                      QString(error.name()+": "+error.message()));
    }
}

void Solid::Backends::Hal::StorageAccess::slotProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    Q_UNUSED(exitStatus);
    if (m_setupInProgress) {
        m_setupInProgress = false;

        if (exitCode==0) {
            m_device->broadcastActionDone("setup");
        } else {
            m_device->broadcastActionDone("setup", Solid::UnauthorizedOperation,
                                          m_process->readAllStandardError());
        }
    } else if (m_teardownInProgress) {
        m_teardownInProgress = false;
        if (exitCode==0) {
            m_device->broadcastActionDone("teardown");
        } else {
            m_device->broadcastActionDone("teardown", Solid::UnauthorizedOperation,
                                          m_process->readAllStandardError());
        }
    } else if (m_ejectInProgress) {
        if (exitCode==0)  {
            m_ejectInProgress = false;
            m_device->broadcastActionDone("eject");
        } else {
            callHalVolumeEject();
        }
    }

    delete m_process;
}

void StorageAccess::slotSetupRequested()
{
    m_setupInProgress = true;
    emit setupRequested(m_device->udi());
}

void StorageAccess::slotTeardownRequested()
{
    m_teardownInProgress = true;
    emit teardownRequested(m_device->udi());
}

void StorageAccess::slotEjectRequested()
{
    m_ejectInProgress = true;
}

QString generateReturnObjectPath()
{
    static int number = 1;

    return "/org/kde/solid/HalStorageAccess_"+QString::number(number++);
}

bool StorageAccess::requestPassphrase()
{
    QString udi = m_device->udi();
    QString returnService = QDBusConnection::sessionBus().baseService();
    m_lastReturnObject = generateReturnObjectPath();

    QDBusConnection::sessionBus().registerObject(m_lastReturnObject, this,
                                                 QDBusConnection::ExportScriptableSlots);


    QWidget *activeWindow = QApplication::activeWindow();
    uint wId = 0;
    if (activeWindow!=0) {
        wId = (uint)activeWindow->winId();
    }

    QString appId = QCoreApplication::applicationName();

    QDBusInterface soliduiserver("org.kde.kded", "/modules/soliduiserver", "org.kde.SolidUiServer");
    QDBusReply<void> reply = soliduiserver.call("showPassphraseDialog", udi,
                                                returnService, m_lastReturnObject,
                                                wId, appId);
    m_passphraseRequested = reply.isValid();
    if (!m_passphraseRequested) {
        qWarning() << "Failed to call the SolidUiServer, D-Bus said:" << reply.error();
    }
    return m_passphraseRequested;
}

void StorageAccess::passphraseReply(const QString &passphrase)
{
    if (m_passphraseRequested) {
        QDBusConnection::sessionBus().unregisterObject(m_lastReturnObject);
        m_passphraseRequested = false;
        if (!passphrase.isEmpty()) {
            callCryptoSetup(passphrase);
        } else {
            m_setupInProgress = false;
            m_device->broadcastActionDone("setup");
        }
    }
}

bool StorageAccess::callHalVolumeMount()
{
    QDBusConnection c = QDBusConnection::systemBus();
    QString udi = m_device->udi();
    QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Hal", udi,
                                                      "org.freedesktop.Hal.Device.Volume",
                                                      "Mount");

    // HAL 0.5.12 supports using alternative drivers for the same filesystem.
    // This is mainly used to integrate the ntfs-3g driver.
    // Unfortunately, the primary driver gets used unless we
    // specify some other driver (fstype) to the Mount method.
    // TODO: Allow the user to choose the driver.

    QString fstype = m_device->prop("volume.fstype").toString();
    QStringList halOptions = m_device->prop("volume.mount.valid_options").toStringList();

    QString alternativePreferred = m_device->prop("volume.fstype.alternative.preferred").toString();
    if (!alternativePreferred.isEmpty()) {
        QStringList alternativeFstypes = m_device->prop("volume.fstype.alternative").toStringList();
        if (alternativeFstypes.contains(alternativePreferred)) {
            fstype = alternativePreferred;
            halOptions = m_device->prop("volume.mount."+fstype+".valid_options").toStringList();
        }
    }

    QStringList options;

#ifdef Q_OS_FREEBSD
    QString uid="-u=";
#else
    QString uid="uid=";
#endif
    if (halOptions.contains(uid)) {
        options << uid+QString::number(::getuid());
    }

#ifdef Q_OS_FREEBSD
    char *cType;
    if ( fstype=="vfat" && halOptions.contains("-L=")) {
        if ( (cType = getenv("LC_ALL")) || (cType = getenv("LC_CTYPE")) || (cType = getenv("LANG")) )
              options << "-L="+QString(cType);
    }
    else if ( (fstype.startsWith(QLatin1String("ntfs")) || fstype=="iso9660" || fstype=="udf") && halOptions.contains("-C=") ) {
        if ((cType = getenv("LC_ALL")) || (cType = getenv("LC_CTYPE")) || (cType = getenv("LANG")) )
            options << "-C="+QString(nl_langinfo(CODESET));
    }
#else
    if (fstype=="vfat" || fstype=="ntfs" || fstype=="iso9660" || fstype=="udf" ) {
        if (halOptions.contains("utf8"))
            options<<"utf8";
        else if (halOptions.contains("iocharset="))
            options<<"iocharset=utf8";
        if (halOptions.contains("shortname="))
            options<<"shortname=mixed";
        if (halOptions.contains("flush"))
            options<<"flush";
    }
    // pass our locale to the ntfs-3g driver so it can translate local characters
    else if ( halOptions.contains("locale=") ) {
        // have to obtain LC_CTYPE as returned by the `locale` command
        // check in the same order as `locale` does
        char *cType;
        if ( (cType = getenv("LC_ALL")) || (cType = getenv("LC_CTYPE")) || (cType = getenv("LANG")) ) {
            options << "locale="+QString(cType);
        }
    }
#endif

    msg << "" << fstype << options;

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(QDBusMessage)),
                              SLOT(slotDBusError(QDBusError)));
}

bool StorageAccess::callHalVolumeUnmount()
{
    QDBusConnection c = QDBusConnection::systemBus();
    QString udi = m_device->udi();
    QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Hal", udi,
                                                      "org.freedesktop.Hal.Device.Volume",
                                                      "Unmount");

    msg << QStringList();

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(QDBusMessage)),
                              SLOT(slotDBusError(QDBusError)));
}

bool StorageAccess::callHalVolumeEject()
{
    QString udi = m_device->udi();
    QString interface = "org.freedesktop.Hal.Device.Volume";

    QDBusConnection c = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Hal", udi,
                                                      interface, "Eject");

    msg << QStringList();

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(QDBusMessage)),
                              SLOT(slotDBusError(QDBusError)));
}

bool Solid::Backends::Hal::StorageAccess::callSystemMount()
{
    const QString device = m_device->prop("block.device").toString();
    m_process = FstabHandling::callSystemCommand("mount", device,
                                                 this, SLOT(slotProcessFinished(int,QProcess::ExitStatus)));

    return m_process!=0;
}

bool Solid::Backends::Hal::StorageAccess::callSystemUnmount()
{
    const QString device = m_device->prop("block.device").toString();
    m_process = FstabHandling::callSystemCommand("umount", device,
                                                 this, SLOT(slotProcessFinished(int,QProcess::ExitStatus)));

    return m_process!=0;
}

void StorageAccess::callCryptoSetup(const QString &passphrase)
{
    QDBusConnection c = QDBusConnection::systemBus();
    QString udi = m_device->udi();
    QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Hal", udi,
                                                      "org.freedesktop.Hal.Device.Volume.Crypto",
                                                      "Setup");

    msg << passphrase;

    c.callWithCallback(msg, this,
                       SLOT(slotDBusReply(QDBusMessage)),
                       SLOT(slotDBusError(QDBusError)));
}

bool StorageAccess::callCryptoTeardown()
{
    QDBusConnection c = QDBusConnection::systemBus();
    QString udi = m_device->udi();
    QDBusMessage msg = QDBusMessage::createMethodCall("org.freedesktop.Hal", udi,
                                                      "org.freedesktop.Hal.Device.Volume.Crypto",
                                                      "Teardown");

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(QDBusMessage)),
                              SLOT(slotDBusError(QDBusError)));
}

//#include "backends/hal/halstorageaccess.moc"
