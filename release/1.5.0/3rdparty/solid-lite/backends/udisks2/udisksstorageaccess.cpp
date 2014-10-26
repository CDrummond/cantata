/*
    Copyright 2009 Pino Toscano <pino@kde.org>
    Copyright 2009-2012 Lukáš Tinkl <ltinkl@redhat.com>

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
#include "udisks2.h"

#include <QtDBus>
#include <QApplication>
#include <QWidget>
#include <QDomDocument>

using namespace Solid::Backends::UDisks2;

StorageAccess::StorageAccess(Device *device)
    : DeviceInterface(device), m_setupInProgress(false), m_teardownInProgress(false), m_passphraseRequested(false)
{
    connect(device, SIGNAL(changed()), this, SLOT(checkAccessibility()));
    updateCache();

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
                             SLOT(slotSetupDone(int, const QString&)));

    m_device->registerAction("teardown", this,
                             SLOT(slotTeardownRequested()),
                             SLOT(slotTeardownDone(int, const QString&)));
}

bool StorageAccess::isLuksDevice() const
{
    return m_device->isEncryptedContainer(); // encrypted device
}

bool StorageAccess::isAccessible() const
{
    if (isLuksDevice()) { // check if the cleartext slave is mounted
        const QString path = clearTextPath();
        //qDebug() << Q_FUNC_INFO << "CLEARTEXT device path: " << path;
        if (path.isEmpty() || path == "/")
            return false;
        Device holderDevice(path);
        return holderDevice.isMounted();
    }

    return m_device->isMounted();
}

QString StorageAccess::filePath() const
{
    QByteArrayList mntPoints;

    if (isLuksDevice()) {  // encrypted (and unlocked) device
        const QString path = clearTextPath();
        if (path.isEmpty() || path == "/")
            return QString();
        Device holderDevice(path);
        mntPoints = qdbus_cast<QByteArrayList>(holderDevice.prop("MountPoints"));
        if (!mntPoints.isEmpty())
            return QFile::decodeName(mntPoints.first()); // FIXME Solid doesn't support multiple mount points
        else
            return QString();
    }

    mntPoints = qdbus_cast<QByteArrayList>(m_device->prop("MountPoints"));

    if (!mntPoints.isEmpty())
        return QFile::decodeName(mntPoints.first()); // FIXME Solid doesn't support multiple mount points
    else
        return QString();
}

bool StorageAccess::isIgnored() const
{
    return m_device->prop("HintIgnore").toBool();
}

bool StorageAccess::setup()
{
    if ( m_teardownInProgress || m_setupInProgress )
        return false;
    m_setupInProgress = true;
    m_device->broadcastActionRequested("setup");

    if (m_device->isEncryptedContainer() && clearTextPath().isEmpty())
        return requestPassphrase();
    else
        return mount();
}

bool StorageAccess::teardown()
{
    if ( m_teardownInProgress || m_setupInProgress )
        return false;
    m_teardownInProgress = true;
    m_device->broadcastActionRequested("teardown");

    return unmount();
}

void StorageAccess::updateCache()
{
    m_isAccessible = isAccessible();
}

void StorageAccess::checkAccessibility()
{
    const bool old_isAccessible = m_isAccessible;
    updateCache();

    if (old_isAccessible != m_isAccessible) {
        Q_EMIT accessibilityChanged(m_isAccessible, m_device->udi());
    }
}

void StorageAccess::slotDBusReply( const QDBusMessage & /*reply*/ )
{
    const QString ctPath = clearTextPath();
    if (m_setupInProgress)
    {
        if (isLuksDevice() && !isAccessible()) { // unlocked device, now mount it
            mount();
        }
        else // Don't broadcast setupDone unless the setup is really done. (Fix kde#271156)
        {
            m_setupInProgress = false;
            m_device->broadcastActionDone("setup");

            checkAccessibility();
        }
    }
    else if (m_teardownInProgress)  // FIXME
    {
        if (isLuksDevice() && !ctPath.isEmpty() && ctPath != "/") // unlocked device, lock it
        {
            callCryptoTeardown();
        }
        else if (!ctPath.isEmpty() && ctPath != "/") {
            callCryptoTeardown(true); // Lock crypted parent
        }
        else
        {
            // try to "eject" (aka safely remove) from the (parent) drive, e.g. SD card from a reader
            QString drivePath = m_device->drivePath();
            if (!drivePath.isEmpty() || drivePath != "/")
            {
                Device drive(drivePath);
                if (drive.prop("Ejectable").toBool() &&
                        drive.prop("MediaAvailable").toBool() &&
                        !m_device->isOpticalDisc()) // optical drives have their Eject method
                {
                    QDBusConnection c = QDBusConnection::systemBus();
                    QDBusMessage msg = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, drivePath, UD2_DBUS_INTERFACE_DRIVE, "Eject");
                    msg << QVariantMap();   // options, unused now
                    c.call(msg, QDBus::NoBlock);
                }
            }

            m_teardownInProgress = false;
            m_device->broadcastActionDone("teardown");

            checkAccessibility();
        }
    }
}

void StorageAccess::slotDBusError( const QDBusError & error )
{
    //qDebug() << Q_FUNC_INFO << "DBUS ERROR:" << error.name() << error.message();

    if (m_setupInProgress)
    {
        m_setupInProgress = false;
        m_device->broadcastActionDone("setup", m_device->errorToSolidError(error.name()),
                                      m_device->errorToString(error.name()) + ": " +error.message());

        checkAccessibility();
    }
    else if (m_teardownInProgress)
    {
        m_teardownInProgress = false;
        m_device->broadcastActionDone("teardown", m_device->errorToSolidError(error.name()),
                                      m_device->errorToString(error.name()) + ": " + error.message());
        checkAccessibility();
    }
}

void StorageAccess::slotSetupRequested()
{
    m_setupInProgress = true;
    //qDebug() << "SETUP REQUESTED:" << m_device->udi();
    Q_EMIT setupRequested(m_device->udi());
}

void StorageAccess::slotSetupDone(int error, const QString &errorString)
{
    m_setupInProgress = false;
    //qDebug() << "SETUP DONE:" << m_device->udi();
    Q_EMIT setupDone(static_cast<Solid::ErrorType>(error), errorString, m_device->udi());

    checkAccessibility();
}

void StorageAccess::slotTeardownRequested()
{
    m_teardownInProgress = true;
    Q_EMIT teardownRequested(m_device->udi());
}

void StorageAccess::slotTeardownDone(int error, const QString &errorString)
{
    m_teardownInProgress = false;
    Q_EMIT teardownDone(static_cast<Solid::ErrorType>(error), errorString, m_device->udi());

    checkAccessibility();
}

bool StorageAccess::mount()
{
    QString path = m_device->udi();
    const QString ctPath = clearTextPath();

    if (isLuksDevice() && !ctPath.isEmpty()) { // mount options for the cleartext volume
        path = ctPath;
    }

    QDBusConnection c = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, path, UD2_DBUS_INTERFACE_FILESYSTEM, "Mount");
    QVariantMap options;

    if (m_device->prop("IdType").toString() == "vfat")
        options.insert("options", "flush");

    msg << options;

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(const QDBusMessage &)),
                              SLOT(slotDBusError(const QDBusError &)));
}

bool StorageAccess::unmount()
{
    QString path = m_device->udi();
    const QString ctPath = clearTextPath();

    if (isLuksDevice() && !ctPath.isEmpty()) { // unmount options for the cleartext volume
        path = ctPath;
    }

    QDBusConnection c = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, path, UD2_DBUS_INTERFACE_FILESYSTEM, "Unmount");

    msg << QVariantMap();   // options, unused now

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(const QDBusMessage &)),
                              SLOT(slotDBusError(const QDBusError &)),
                              s_unmountTimeout);
}

QString StorageAccess::generateReturnObjectPath()
{
    static int number = 1;

    return "/org/kde/solid/UDisks2StorageAccess_"+QString::number(number++);
}

QString StorageAccess::clearTextPath() const
{
    const QString prefix = "/org/freedesktop/UDisks2/block_devices";
    QDBusMessage call = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, prefix,
                                                       DBUS_INTERFACE_INTROSPECT, "Introspect");
    QDBusPendingReply<QString> reply = QDBusConnection::systemBus().asyncCall(call);
    reply.waitForFinished();

    if (reply.isValid()) {
        QDomDocument dom;
        dom.setContent(reply.value());
        QDomNodeList nodeList = dom.documentElement().elementsByTagName("node");
        for (int i = 0; i < nodeList.count(); i++) {
            QDomElement nodeElem = nodeList.item(i).toElement();
            if (!nodeElem.isNull() && nodeElem.hasAttribute("name")) {
                const QString udi = prefix + "/" + nodeElem.attribute("name");
                Device holderDevice(udi);

                if (m_device->udi() == holderDevice.prop("CryptoBackingDevice").value<QDBusObjectPath>().path()) {
                    //qDebug() << Q_FUNC_INFO << "CLEARTEXT device path: " << udi;
                    return udi;
                }
            }
        }
    }

    return QString();
}

bool StorageAccess::requestPassphrase()
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

void StorageAccess::passphraseReply(const QString & passphrase)
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

void StorageAccess::callCryptoSetup(const QString & passphrase)
{
    QDBusConnection c = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE, m_device->udi(), UD2_DBUS_INTERFACE_ENCRYPTED, "Unlock");

    msg << passphrase;
    msg << QVariantMap();   // options, unused now

    c.callWithCallback(msg, this,
                       SLOT(slotDBusReply(const QDBusMessage &)),
                       SLOT(slotDBusError(const QDBusError &)));
}

bool StorageAccess::callCryptoTeardown(bool actOnParent)
{
    QDBusConnection c = QDBusConnection::systemBus();
    QDBusMessage msg = QDBusMessage::createMethodCall(UD2_DBUS_SERVICE,
                                                      actOnParent ? (m_device->prop("CryptoBackingDevice").value<QDBusObjectPath>().path()) : m_device->udi(),
                                                      UD2_DBUS_INTERFACE_ENCRYPTED, "Lock");
    msg << QVariantMap();   // options, unused now

    return c.callWithCallback(msg, this,
                              SLOT(slotDBusReply(const QDBusMessage &)),
                              SLOT(slotDBusError(const QDBusError &)));
}
