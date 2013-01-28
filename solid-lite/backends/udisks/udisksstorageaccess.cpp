/*
    Copyright 2009 Pino Toscano <pino@kde.org>
    Copyright 2009, 2011 Lukas Tinkl <ltinkl@redhat.com>

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

#include "udisksstorageaccess.h"
#include "udisks.h"

#include <QProcess>
#include <QtDBus>
#include <QApplication>
#include <QWidget>

using namespace Solid::Backends::UDisks;

UDisksStorageAccess::UDisksStorageAccess(UDisksDevice *device)
    : DeviceInterface(device), m_setupInProgress(false), m_teardownInProgress(false), m_passphraseRequested(false)
{
    connect(device, SIGNAL(changed()), this, SLOT(slotChanged()));
    updateCache();

    // Delay connecting to DBus signals to avoid the related time penalty
    // in hot paths such as predicate matching
    QTimer::singleShot(0, this, SLOT(connectDBusSignals()));
}

UDisksStorageAccess::~UDisksStorageAccess()
{
}

void UDisksStorageAccess::connectDBusSignals()
{
    m_device->registerAction("setup", this,
                             SLOT(slotSetupRequested()),
                             SLOT(slotSetupDone(int,QString)));

    m_device->registerAction("teardown", this,
                             SLOT(slotTeardownRequested()),
                             SLOT(slotTeardownDone(int,QString)));
}

bool UDisksStorageAccess::isLuksDevice() const
{
    return m_device->prop("DeviceIsLuks").toBool();
}

bool UDisksStorageAccess::isAccessible() const
{
    if (isLuksDevice()) { // check if the cleartext slave is mounted
        UDisksDevice holderDevice(m_device->prop("LuksHolder").value<QDBusObjectPath>().path());
        return holderDevice.prop("DeviceIsMounted").toBool();
    }

    return m_device->prop("DeviceIsMounted").toBool();
}

QString UDisksStorageAccess::filePath() const
{
    if (!isAccessible())
        return QString();

    QStringList mntPoints;

    if (isLuksDevice()) {  // encrypted (and unlocked) device
        QString path = m_device->prop("LuksHolder").value<QDBusObjectPath>().path();
        if (path.isEmpty() || path == "/")
            return QString();
        UDisksDevice holderDevice(path);
        mntPoints = holderDevice.prop("DeviceMountPaths").toStringList();
        if (!mntPoints.isEmpty())
            return mntPoints.first(); // FIXME Solid doesn't support multiple mount points
        else
            return QString();
    }

    mntPoints = m_device->prop("DeviceMountPaths").toStringList();

    if (!mntPoints.isEmpty())
        return mntPoints.first(); // FIXME Solid doesn't support multiple mount points
    else
        return QString();
}

bool UDisksStorageAccess::isIgnored() const
{
    return m_device->isDeviceBlacklisted();
}

bool UDisksStorageAccess::setup()
{
    if ( m_teardownInProgress || m_setupInProgress )
        return false;
    m_setupInProgress = true;
    m_device->broadcastActionRequested("setup");

    if (m_device->prop("IdUsage").toString() == "crypto")
        return requestPassphrase();
    else
        return mount();
}

bool UDisksStorageAccess::teardown()
{
    if ( m_teardownInProgress || m_setupInProgress )
        return false;
    m_teardownInProgress = true;
    m_device->broadcastActionRequested("teardown");

    return unmount();
}

void UDisksStorageAccess::slotChanged()
{
    const bool old_isAccessible = m_isAccessible;
    updateCache();

    if (old_isAccessible != m_isAccessible)
    {
        emit accessibilityChanged(m_isAccessible, m_device->udi());
    }
}

void UDisksStorageAccess::updateCache()
{
    m_isAccessible = isAccessible();
}

void UDisksStorageAccess::slotDBusReply( const QDBusMessage & reply )
{
    Q_UNUSED(reply);
    if (m_setupInProgress)
    {
        if (isLuksDevice() && !isAccessible())  // unlocked device, now mount it
            mount();

        else // Don't broadcast setupDone unless the setup is really done. (Fix kde#271156)
        {
            m_setupInProgress = false;
            m_device->broadcastActionDone("setup");
        }
    }
    else if (m_teardownInProgress)
    {
        QString clearTextPath =  m_device->prop("LuksHolder").value<QDBusObjectPath>().path();
        if (isLuksDevice() && clearTextPath != "/") // unlocked device, lock it
        {
            callCryptoTeardown();
        }
        else if (m_device->prop("DeviceIsLuksCleartext").toBool()) {
            callCryptoTeardown(true); // Lock crypted parent
        }
        else
        {
            if (m_device->prop("DriveIsMediaEjectable").toBool() &&
                    m_device->prop("DeviceIsMediaAvailable").toBool() &&
                    !m_device->prop("DeviceIsOpticalDisc").toBool()) // optical drives have their Eject method
            {
                QString devnode = m_device->prop("DeviceFile").toString();

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

                QProcess::startDetached( program, args );
            }

            // try to eject the (parent) drive, e.g. SD card from a reader
            QString drivePath = m_device->prop("PartitionSlave").value<QDBusObjectPath>().path();
            if (!drivePath.isEmpty() || drivePath != "/")
            {
                QDBusConnection c = QDBusConnection::systemBus();
                QDBusMessage msg = QDBusMessage::createMethodCall(UD_DBUS_SERVICE, drivePath, UD_DBUS_INTERFACE_DISKS_DEVICE, "DriveEject");
                msg << QStringList();   // options, unused now
                c.call(msg, QDBus::NoBlock);
            }

            m_teardownInProgress = false;
            m_device->broadcastActionDone("teardown");
        }
    }
}

void UDisksStorageAccess::slotDBusError( const QDBusError & error )
{
    if (m_setupInProgress)
    {
        m_setupInProgress = false;
        m_device->broadcastActionDone("setup", m_device->errorToSolidError(error.name()),
                                      m_device->errorToString(error.name()) + ": " +error.message());

    }
    else if (m_teardownInProgress)
    {
        m_teardownInProgress = false;
        m_device->broadcastActionDone("teardown", m_device->errorToSolidError(error.name()),
                                      m_device->errorToString(error.name()) + ": " + error.message());
    }
}

void UDisksStorageAccess::slotSetupRequested()
{
    m_setupInProgress = true;
    emit setupRequested(m_device->udi());
}

void UDisksStorageAccess::slotSetupDone(int error, const QString &errorString)
{
    m_setupInProgress = false;
    emit setupDone(static_cast<Solid::ErrorType>(error), errorString, m_device->udi());
    slotChanged();
}

void UDisksStorageAccess::slotTeardownRequested()
{
    m_teardownInProgress = true;
    emit teardownRequested(m_device->udi());
}

void UDisksStorageAccess::slotTeardownDone(int error, const QString &errorString)
{
    m_teardownInProgress = false;
    emit teardownDone(static_cast<Solid::ErrorType>(error), errorString, m_device->udi());
    slotChanged();
}

bool UDisksStorageAccess::mount()
{
    QString path = m_device->udi();
    if (path.endsWith(":media")) {
        path.chop(6);
    }
    QString fstype;
    QStringList options;

    if (isLuksDevice()) { // mount options for the cleartext volume
        path = m_device->prop("LuksHolder").value<QDBusObjectPath>().path();
        UDisksDevice holderDevice(path);
        fstype = holderDevice.prop("IdType").toString();
    }

    QDBusConnection c = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(UD_DBUS_SERVICE, path, UD_DBUS_INTERFACE_DISKS_DEVICE, "FilesystemMount");

    if (m_device->prop("IdUsage").toString() == "filesystem")
        fstype = m_device->prop("IdType").toString();

    if (fstype == "vfat") {
        options << "flush";
    }

    msg << fstype;
    msg << options;

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(QDBusMessage)),
                              SLOT(slotDBusError(QDBusError)));
}

bool UDisksStorageAccess::unmount()
{
    QString path = m_device->udi();
    if (path.endsWith(":media")) {
        path.chop(6);
    }

    if (isLuksDevice()) { // unmount options for the cleartext volume
        path = m_device->prop("LuksHolder").value<QDBusObjectPath>().path();
    }

    QDBusConnection c = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(UD_DBUS_SERVICE, path, UD_DBUS_INTERFACE_DISKS_DEVICE, "FilesystemUnmount");

    msg << QStringList();   // options, unused now

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(QDBusMessage)),
                              SLOT(slotDBusError(QDBusError)),
                              s_unmountTimeout);
}

QString UDisksStorageAccess::generateReturnObjectPath()
{
    static int number = 1;

    return "/org/kde/solid/UDisksStorageAccess_"+QString::number(number++);
}

bool UDisksStorageAccess::requestPassphrase()
{
    QString udi = m_device->udi();
    QString returnService = QDBusConnection::sessionBus().baseService();
    m_lastReturnObject = generateReturnObjectPath();

    QDBusConnection::sessionBus().registerObject(m_lastReturnObject, this, QDBusConnection::ExportScriptableSlots);

    QWidget *activeWindow = QApplication::activeWindow();
    uint wId = 0;
    if (activeWindow!=0)
        wId = (uint)activeWindow->winId();

    QString appId = QCoreApplication::applicationName();

    QDBusInterface soliduiserver("org.kde.kded", "/modules/soliduiserver", "org.kde.SolidUiServer");
    QDBusReply<void> reply = soliduiserver.call("showPassphraseDialog", udi, returnService,
                                                m_lastReturnObject, wId, appId);
    m_passphraseRequested = reply.isValid();
    if (!m_passphraseRequested)
        qWarning() << "Failed to call the SolidUiServer, D-Bus said:" << reply.error();

    return m_passphraseRequested;
}

void UDisksStorageAccess::passphraseReply( const QString & passphrase )
{
    if (m_passphraseRequested)
    {
        QDBusConnection::sessionBus().unregisterObject(m_lastReturnObject);
        m_passphraseRequested = false;
        if (!passphrase.isEmpty())
            callCryptoSetup(passphrase);
        else
        {
            m_setupInProgress = false;
            m_device->broadcastActionDone("setup");
        }
    }
}

void UDisksStorageAccess::callCryptoSetup( const QString & passphrase )
{
    QDBusConnection c = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(UD_DBUS_SERVICE, m_device->udi(), UD_DBUS_INTERFACE_DISKS_DEVICE, "LuksUnlock");

    msg << passphrase;
    msg << QStringList();   // options, unused now

    c.callWithCallback(msg, this,
                       SLOT(slotDBusReply(QDBusMessage)),
                       SLOT(slotDBusError(QDBusError)));
}

bool UDisksStorageAccess::callCryptoTeardown(bool actOnParent)
{
    QDBusConnection c = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(UD_DBUS_SERVICE,
                        actOnParent?(m_device->prop("LuksCleartextSlave").value<QDBusObjectPath>().path()):m_device->udi(),
                        UD_DBUS_INTERFACE_DISKS_DEVICE, "LuksLock");
    msg << QStringList();   // options, unused now

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(QDBusMessage)),
                              SLOT(slotDBusError(QDBusError)));
}

//#include "backends/udisks/udisksstorageaccess.moc"
