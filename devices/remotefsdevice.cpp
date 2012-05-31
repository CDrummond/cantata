/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,f
 * Boston, MA 02110-1301, USA.
 */

#include "remotefsdevice.h"
#include "config.h"
#ifdef ENABLE_KIO_REMOTE_DEVICES
#include "remotekiodevice.h"
#endif
#include "mpdparseutils.h"
#include "remotedevicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "actiondialog.h"
#include "network.h"
#include "httpserver.h"
#include "localize.h"
#include <QtCore/QTimer>
#include <QtCore/QProcess>
#include <KDE/KUrl>
#include <KDE/KDiskFreeSpaceInfo>
#include <KDE/KGlobal>
#include <KDE/KConfigGroup>
#include <KDE/KStandardDirs>
#include <kmountpoint.h>
#include <kde_file.h>
#include <stdio.h>
#include <unistd.h>

const QLatin1String RemoteFsDevice::constSshfsProtocol("cantata-sshfs");

static QString mountPoint(const RemoteFsDevice::Details &details, bool create)
{
    if (details.url.isLocalFile()) {
        return details.url.path();
    }
    return Network::cacheDir(QLatin1String("mount/")+details.name, create);
}

void RemoteFsDevice::Details::load(const QString &group)
{
    KConfigGroup grp(KGlobal::config(), group);
    name=grp.readEntry("name", name);
    url=grp.readEntry("url", url);
    if (url.isEmpty()) {
        // Old, pre 0.7.0 remote device...
        QString folder=grp.readEntry("folder", QString());
        if (!folder.isEmpty()) {
            if (1==grp.readEntry("protocol", 0)) {
                QString host=grp.readEntry("host", QString());
                QString user=grp.readEntry("user", QString());
                int port=grp.readEntry("port", 0);

                url=KUrl(RemoteFsDevice::constSshfsProtocol+QLatin1String("://")+ (user.isEmpty() ? QString() : (user+QChar('@')))
                                + host + (port<=0 ? QString() : QString(QChar(':')+QString::number(port)))
                                + (folder.startsWith("/") ? folder : (folder.isEmpty() ? QString("/") : folder)));
            } else {
                url=KUrl(QLatin1String("file://")+(folder.startsWith("/") ? folder : (folder.isEmpty() ? QString("/") : folder)));
            }
        }
        grp.deleteEntry("protocol");
        grp.deleteEntry("folder");
        grp.deleteEntry("host");
        grp.deleteEntry("user");
        grp.deleteEntry("port");
        grp.writeEntry("url", url);
    }
}

void RemoteFsDevice::Details::save(const QString &group) const
{
    KConfigGroup grp(KGlobal::config(), group);
    grp.writeEntry("name", name);
    grp.writeEntry("url", url);
    grp.sync();
}


static const QLatin1String constCfgPrefix("RemoteDevice-");
static const QLatin1String constCfgKey("remoteDevices");

QList<Device *> RemoteFsDevice::loadAll(DevicesModel *m)
{
    QList<Device *> devices;
    KConfigGroup grp(KGlobal::config(), "General");
    QStringList names=grp.readEntry(constCfgKey, QStringList());
    foreach (const QString &n, names) {
        Details d;
        d.load(constCfgPrefix+n);
        if (d.isEmpty() || d.name!=n) {
            KGlobal::config()->deleteGroup(constCfgPrefix+n);
        } else if (d.url.isLocalFile() || constSshfsProtocol==d.url.protocol()) {
            devices.append(new RemoteFsDevice(m, d));
        }
        #ifdef ENABLE_KIO_REMOTE_DEVICES
        else {
            devices.append(new RemoteKioDevice(m, d));
        }
        #endif
    }
    if (devices.count()!=names.count()) {
        KGlobal::config()->sync();
    }
    return devices;
}

Device * RemoteFsDevice::create(DevicesModel *m, const QString &cover, const Options &options, const Details &d)
{
    if (d.isEmpty()) {
        return false;
    }
    KConfigGroup grp(KGlobal::config(), "General");
    QStringList names=grp.readEntry(constCfgKey, QStringList());
    if (names.contains(d.name)) {
        return false;
    }
    names.append(d.name);
    grp.writeEntry(constCfgKey, names);
    d.save(constCfgPrefix+d.name);
    if (d.url.isLocalFile() || constSshfsProtocol==d.url.protocol()) {
        return new RemoteFsDevice(m, cover, options, d);
    }
    #ifdef ENABLE_KIO_REMOTE_DEVICES
    return new RemoteKioDevice(m, cover, options, d);
    #else
    return 0;
    #endif
}

void RemoteFsDevice::remove(Device *dev)
{
    if (!dev || !(RemoteFs==dev->devType() || RemoteKio==dev->devType())) {
        return;
    }
    KConfigGroup grp(KGlobal::config(), "General");
    QStringList names=grp.readEntry(constCfgKey, QStringList());
    RemoteFsDevice *rfs=qobject_cast<RemoteFsDevice *>(dev);
    #ifdef ENABLE_KIO_REMOTE_DEVICES
    RemoteKioDevice *rkio=rfs ? 0 : qobject_cast<RemoteKioDevice *>(dev);
    QString name=rfs ? rfs->details.name : (rkio ? rkio->details.name : QString());
    #else
    QString name=rfs ? rfs->details.name : QString();
    #endif
    if (names.contains(name)) {
        names.removeAll(name);
        KGlobal::config()->deleteGroup(dev->udi());
        grp.writeEntry(constCfgKey, names);
        KGlobal::config()->sync();
    }
    if (rfs) {
        rfs->stopScanner(false);
        if (rfs->isConnected()) {
            rfs->unmount();
        }
    }
    #ifdef ENABLE_KIO_REMOTE_DEVICES
    else if (rkio) {
        rkio->stopScanner(false);
    }
    #endif
    dev->deleteLater();
}

QString RemoteFsDevice::createUdi(const QString &n)
{
    return constCfgPrefix+n;
}

void RemoteFsDevice::renamed(const QString &oldName, const QString &newName)
{
    KConfigGroup grp(KGlobal::config(), "General");
    QStringList names=grp.readEntry(constCfgKey, QStringList());
    if (names.contains(oldName)) {
        names.removeAll(oldName);
        KGlobal::config()->deleteGroup(createUdi(oldName));
    }
    if (!names.contains(newName)) {
        names.append(newName);
    }
    grp.writeEntry(constCfgKey, names);
    KGlobal::config()->sync();
}

RemoteFsDevice::RemoteFsDevice(DevicesModel *m, const QString &cover, const Options &options, const Details &d)
    : FsDevice(m, d.name)
    , lastCheck(0)
    , details(d)
    , proc(0)
{
    coverFileName=cover;
    opts=options;
    details.url.setPath(MPDParseUtils::fixPath(details.url.path()));
    load();
    mount();
}

RemoteFsDevice::RemoteFsDevice(DevicesModel *m, const Details &d)
    : FsDevice(m, d.name)
    , lastCheck(0)
    , details(d)
    , proc(0)
{
    details.url.setPath(MPDParseUtils::fixPath(details.url.path()));
    setup();
}

RemoteFsDevice::~RemoteFsDevice() {
    stopScanner(false);
}

void RemoteFsDevice::toggle()
{
    if (isConnected()) {
        unmount();
    } else {
        mount();
    }
}

void RemoteFsDevice::mount()
{
    if (details.isLocalFile()) {
        return;
    }
    if (isConnected() || proc) {
        return;
    }

    QString cmd;
    QStringList args;
    if (!details.isLocalFile() && !details.isEmpty()) {
        if (ttyname(0)) {
            emit error(i18n("Password prompting does not work when cantata is started from the commandline."));
            return;
        }
        if (KStandardDirs::findExe("ksshaskpass").isEmpty()) {
            emit error(i18n("\"ksshaskpass\" is not installed! This is required for entering passwords"));
            return;
        }
        cmd=KStandardDirs::findExe("sshfs");
        if (!cmd.isEmpty()) {
            if (!QDir(mountPoint(details, true)).entryList(QDir::NoDot|QDir::NoDotDot|QDir::AllEntries|QDir::Hidden).isEmpty()) {
                emit error(i18n("Mount point (\"%1\") is not empty!", mountPoint(details, true)));
                return;
            }
            args << details.url.user()+QChar('@')+details.url.host()+QChar(':')+details.url.path() << QLatin1String("-p")
                    << QString::number(details.url.port()) << mountPoint(details, true)
                    << QLatin1String("-o") << QLatin1String("ServerAliveInterval=15");
                    //<< QLatin1String("-o") << QLatin1String("Ciphers=arcfour");
        } else {
            emit error(i18n("\"sshfs\" is not installed!"));
        }
    }

    if (!cmd.isEmpty()) {
        setStatusMessage(i18n("Connecting..."));
        proc=new QProcess(this);
        proc->setProperty("mount", true);
        connect(proc, SIGNAL(finished(int)), SLOT(procFinished(int)));
        proc->start(cmd, args, QIODevice::ReadOnly);
    }
}

void RemoteFsDevice::unmount()
{
    if (details.isLocalFile()) {
        return;
    }

    if (!isConnected() || proc) {
        return;
    }

    QString cmd;
    QStringList args;
    if (!details.isLocalFile()) {
        QString mp=mountPoint(details, false);
        if (!mp.isEmpty()) {
            cmd=KStandardDirs::findExe("fusermount");
            if (!cmd.isEmpty()) {
                args << QLatin1String("-u") << QLatin1String("-z") << mp;
            } else {
                emit error(i18n("\"fusermount\" is not installed!"));
            }
        }
    }

    if (!cmd.isEmpty()) {
        setStatusMessage(i18n("Disconnecting..."));
        proc=new QProcess(this);
        proc->setProperty("unmount", true);
        connect(proc, SIGNAL(finished(int)), SLOT(procFinished(int)));
        proc->start(cmd, args, QIODevice::ReadOnly);
    }
}

void RemoteFsDevice::procFinished(int exitCode)
{
    bool wasMount=proc->property("mount").isValid();
    proc->deleteLater();
    proc=0;

    if (0!=exitCode) {
        emit error(wasMount ? i18n("Failed to connect to \"%1\"", details.name)
                            : i18n("Failed to disconnect from \"%1\"", details.name));
        setStatusMessage(QString());
    } else if (wasMount) {
        setStatusMessage(i18n("Updating tracks..."));
        load();
    } else {
        setStatusMessage(QString());
        update=new MusicLibraryItemRoot;
        emit updating(udi(), false);
    }
}

bool RemoteFsDevice::isConnected() const
{
    if (details.isLocalFile()) {
        return QDir(details.url.path()).exists();
    }

    QString mp=mountPoint(details, false);
    if (mp.isEmpty()) {
        return false;
    }

    if (opts.useCache && !audioFolder.isEmpty() && QFile::exists(cacheFileName())) {
        return true;
    }

    if (mp.endsWith('/')) {
        mp=mp.left(mp.length()-1);
    }
    KMountPoint::List list=KMountPoint::currentMountPoints();
    foreach (KMountPoint::Ptr p, list) {
        if (p->mountPoint()==mp) {
            return true;
        }
    }
    return false;
}

double RemoteFsDevice::usedCapacity()
{
    if (!isConnected() || !details.isLocalFile()) {
        return -1.0;
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(mountPoint(details, false));
    return inf.size()>0 ? (inf.used()*1.0)/(inf.size()*1.0) : -1.0;
}

QString RemoteFsDevice::capacityString()
{
    if (!isConnected()) {
        return i18n("Not Connected");
    }

    if (!details.isLocalFile()) {
        return i18n("Capacity Unknown");
    }
    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(mountPoint(details, false));
    return i18n("%1 free", KGlobal::locale()->formatByteSize(inf.size()-inf.used()), 1);
}

qint64 RemoteFsDevice::freeSpace()
{
    if (!isConnected() || !details.isLocalFile()) {
        return 0;
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(mountPoint(details, false));
    return inf.size()-inf.used();
}

void RemoteFsDevice::load()
{
    if (isConnected()) {
        setAudioFolder();
        if (!opts.useCache || !readCache()) {
            rescan();
        }
    }
}

void RemoteFsDevice::setup()
{
    QString key=udi();
    opts.load(key);
    details.load(key);
    details.url.setPath(MPDParseUtils::fixPath(details.url.path()));
    KConfigGroup grp(KGlobal::config(), key);
    opts.useCache=grp.readEntry("useCache", true);
    coverFileName=grp.readEntry("coverFileName", "cover.jpg");
    configured=KGlobal::config()->hasGroup(key);
    load();
}

void RemoteFsDevice::setAudioFolder()
{
    audioFolder=mountPoint(details, true);
    if (!audioFolder.endsWith('/')) {
        audioFolder+='/';
    }
}

void RemoteFsDevice::configure(QWidget *parent)
{
    if (isRefreshing()) {
        return;
    }

    RemoteDevicePropertiesDialog *dlg=new RemoteDevicePropertiesDialog(parent);
    connect(dlg, SIGNAL(updatedSettings(const QString &, const Device::Options &, RemoteFsDevice::Details)),
            SLOT(saveProperties(const QString &, const Device::Options &, RemoteFsDevice::Details)));
    if (!configured) {
        connect(dlg, SIGNAL(cancelled()), SLOT(saveProperties()));
    }
    dlg->show(coverFileName, opts, details,
              DevicePropertiesWidget::Prop_All-(DevicePropertiesWidget::Prop_Folder+DevicePropertiesWidget::Prop_AutoScan),
              false, isConnected());
}

bool RemoteFsDevice::canPlaySongs() const
{
    return details.isLocalFile() || HttpServer::self()->isAlive();
}

static inline QString toString(bool b)
{
    return b ? QLatin1String("true") : QLatin1String("false");
}

void RemoteFsDevice::saveOptions()
{
    opts.save(udi());
}

void RemoteFsDevice::saveProperties()
{
    saveProperties(coverFileName, opts, details);
}

void RemoteFsDevice::saveProperties(const QString &newCoverFileName, const Device::Options &newOpts, Details newDetails)
{
    if (configured && opts==newOpts && newCoverFileName==coverFileName && details==newDetails) {
        return;
    }

    configured=true;
    Details oldDetails=details;
    newDetails.url.setPath(MPDParseUtils::fixPath(newDetails.url.path()));

    if (opts.useCache!=newOpts.useCache || newDetails.url!=oldDetails.url) { // Cache/url settings changed
        if (opts.useCache && newDetails.url==oldDetails.url) {
            saveCache();
        } else if (opts.useCache && !newOpts.useCache) {
            removeCache();
        }
    }

    // Name/or URL changed - need to unmount...
    bool newName=!oldDetails.name.isEmpty() && oldDetails.name!=newDetails.name;
    if ((newName || oldDetails.url!=newDetails.url) && isConnected()) {
        unmount();
    }

    opts=newOpts;
    details=newDetails;
    coverFileName=newCoverFileName;
    QString key=udi();
    details.save(key);
    opts.save(key);
    KConfigGroup grp(KGlobal::config(), key);
    grp.writeEntry("useCache", opts.useCache);
    grp.writeEntry("coverFileName", coverFileName);

    if (newName) {
        QString oldMount=mountPoint(oldDetails, false);
        if (!oldMount.isEmpty() && QDir(oldMount).exists()) {
            ::rmdir(QFile::encodeName(oldMount).constData());
        }
        setData(details.name);
        renamed(oldDetails.name, details.name);
        emit udiChanged(createUdi(oldDetails.name), createUdi(details.name));
        m_itemData=details.name;
        setStatusMessage(QString());
    }
}
