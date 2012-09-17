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
#include "mpdparseutils.h"
#include "remotedevicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "actiondialog.h"
#include "network.h"
#include "httpserver.h"
#include "localize.h"
#include "settings.h"
#include <QtCore/QTimer>
#include <QtCore/QProcess>
#include <QtCore/QDir>
#include <QtCore/QFile>
#ifdef ENABLE_KDE_SUPPORT
#include <kmountpoint.h>
#include <kde_file.h>
#endif
#include <stdio.h>
#include <unistd.h>

const QLatin1String RemoteFsDevice::constSshfsProtocol("cantata-sshfs");

static QString mountPoint(const RemoteFsDevice::Details &details, bool create)
{
    if (details.isLocalFile()) {
        return details.url;
    }
    return Network::cacheDir(QLatin1String("mount/")+details.name, create);
}

void RemoteFsDevice::Details::load(const QString &group)
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), group);
    #else
    QSettings cfg;
    cfg.beginGroup(group);
    #endif
    name=GET_STRING("name", name);
    url=GET_STRING("url", url);
    #ifdef ENABLE_KDE_SUPPORT
    if (url.isEmpty()) {
        // Old, pre 0.7.0 remote device...
        QString folder=GET_STRING("folder", QString());
        if (!folder.isEmpty()) {
            if (1==GET_INT("protocol", 0)) {
                QString host=GET_STRING("host", QString());
                QString user=GET_STRING("user", QString());
                int port=GET_INT("port", 0);

                url=RemoteFsDevice::constSshfsProtocol+QLatin1String("://")+ (user.isEmpty() ? QString() : (user+QChar('@')))
                                + host + (port<=0 ? QString() : QString(QChar(':')+QString::number(port)))
                                + (folder.startsWith("/") ? folder : (folder.isEmpty() ? QString("/") : folder));
            } else {
                url=(folder.startsWith("/") ? folder : (folder.isEmpty() ? QString("/") : folder));
            }
        }
        // These are in case of old (KDE-only) entries...
        REMOVE_ENTRY("protocol");
        REMOVE_ENTRY("folder");
        REMOVE_ENTRY("host");
        REMOVE_ENTRY("user");
        REMOVE_ENTRY("port");
        SET_VALUE("url", url);
    }
    if (url.startsWith("file://")) {
        url=url.mid(7);
    }
    #endif
}

void RemoteFsDevice::Details::save(const QString &group) const
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), group);
    #else
    QSettings cfg;
    cfg.beginGroup(group);
    #endif
    SET_VALUE("name", name);
    SET_VALUE("url", url);
    CFG_SYNC;
}

static const QLatin1String constCfgPrefix("RemoteDevice-");
static const QLatin1String constCfgKey("remoteDevices");

QList<Device *> RemoteFsDevice::loadAll(DevicesModel *m)
{
    QList<Device *> devices;
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), "General");
    #else
    QSettings cfg;
    cfg.beginGroup("General");
    #endif
    QStringList names=GET_STRINGLIST(constCfgKey, QStringList());
    foreach (const QString &n, names) {
        Details d;
        d.load(constCfgPrefix+n);
        if (d.isEmpty() || d.name!=n) {
            REMOVE_GROUP(constCfgPrefix+n);
        } else if (d.isLocalFile() || d.url.startsWith(constSshfsProtocol)) {
            devices.append(new RemoteFsDevice(m, d));
        }
    }
    if (devices.count()!=names.count()) {
        CFG_SYNC;
    }
    return devices;
}

Device * RemoteFsDevice::create(DevicesModel *m, const QString &cover, const DeviceOptions &options, const Details &d)
{
    if (d.isEmpty()) {
        return false;
    }
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), "General");
    #else
    QSettings cfg;
    cfg.beginGroup("General");
    #endif
    QStringList names=GET_STRINGLIST(constCfgKey, QStringList());
    if (names.contains(d.name)) {
        return false;
    }
    names.append(d.name);
    SET_VALUE(constCfgKey, names);
    d.save(constCfgPrefix+d.name);
    if (d.isLocalFile() || d.url.startsWith(constSshfsProtocol)) {
        return new RemoteFsDevice(m, cover, options, d);
    }
    return 0;
}

void RemoteFsDevice::remove(Device *dev)
{
    if (!dev || RemoteFs!=dev->devType()) {
        return;
    }
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), "General");
    #else
    QSettings cfg;
    cfg.beginGroup("General");
    #endif
    QStringList names=GET_STRINGLIST(constCfgKey, QStringList());
    RemoteFsDevice *rfs=qobject_cast<RemoteFsDevice *>(dev);
    QString name=rfs ? rfs->details.name : QString();
    if (names.contains(name)) {
        names.removeAll(name);
        REMOVE_GROUP(dev->udi());
        SET_VALUE(constCfgKey, names);
        CFG_SYNC;
    }
    if (rfs) {
        rfs->stopScanner(false);
        if (rfs->isConnected()) {
            rfs->unmount();
        }
    }
    dev->deleteLater();
}

QString RemoteFsDevice::createUdi(const QString &n)
{
    return constCfgPrefix+n;
}

void RemoteFsDevice::renamed(const QString &oldName, const QString &newName)
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), "General");
    #else
    QSettings cfg;
    cfg.beginGroup("General");
    #endif
    QStringList names=GET_STRINGLIST(constCfgKey, QStringList());
    if (names.contains(oldName)) {
        names.removeAll(oldName);
        REMOVE_GROUP(createUdi(oldName));
    }
    if (!names.contains(newName)) {
        names.append(newName);
    }
    SET_VALUE(constCfgKey, names);
    CFG_SYNC;
}

RemoteFsDevice::RemoteFsDevice(DevicesModel *m, const QString &cover, const DeviceOptions &options, const Details &d)
    : FsDevice(m, d.name)
    , lastCheck(0)
    , details(d)
    , proc(0)
{
    coverFileName=cover;
    opts=options;
    details.url=MPDParseUtils::fixPath(details.url);
    load();
    mount();
}

RemoteFsDevice::RemoteFsDevice(DevicesModel *m, const Details &d)
    : FsDevice(m, d.name)
    , lastCheck(0)
    , details(d)
    , proc(0)
{
    details.url=MPDParseUtils::fixPath(details.url);
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
        QStringList askPassList;
        const char *env=qgetenv("KDE_FULL_SESSION");
        QString dm=env && 0==strcmp(env, "true") ? QLatin1String("KDE") : QString(qgetenv("XDG_CURRENT_DESKTOP"));
        if (dm.isEmpty() || QLatin1String("KDE")==dm) {
            askPassList << QLatin1String("ksshaskpass") << QLatin1String("ssh-askpass") << QLatin1String("ssh-askpass-gnome");
        } else {
            askPassList << QLatin1String("ssh-askpass-gnome") << QLatin1String("ssh-askpass") << QLatin1String("ksshaskpass");
        }

        QString askPass;
        foreach (const QString &ap, askPassList) {
            askPass=Utils::findExe(ap);
            if (!askPass.isEmpty()) {
                break;
            }
        }

        if (askPass.isEmpty()) {
            emit error(i18n("No suitable ssh-askpass applicaiton installed! This is required for entering passwords."));
            return;
        }
        cmd=Utils::findExe("sshfs");
        if (!cmd.isEmpty()) {
            if (!QDir(mountPoint(details, true)).entryList(QDir::NoDot|QDir::NoDotDot|QDir::AllEntries|QDir::Hidden).isEmpty()) {
                emit error(i18n("Mount point (\"%1\") is not empty!").arg(mountPoint(details, true)));
                return;
            }
            QUrl url(details.url);
            args << url.userName()+QChar('@')+url.host()+QChar(':')+url.path() << QLatin1String("-p")
                    << QString::number(url.port()) << mountPoint(details, true)
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
            cmd=Utils::findExe("fusermount");
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
        emit error(wasMount ? i18n("Failed to connect to \"%1\"").arg(details.name)
                            : i18n("Failed to disconnect from \"%1\"").arg(details.name));
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
        return QDir(details.url).exists();
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
    #ifdef ENABLE_KDE_SUPPORT
    KMountPoint::List list=KMountPoint::currentMountPoints();
    foreach (KMountPoint::Ptr p, list) {
        if (p->mountPoint()==mp) {
            return true;
        }
    }
    #else
    QFile mtab("/proc/mounts");
    if (mtab.open(QIODevice::ReadOnly)) {
        while (!mtab.atEnd()) {
            QStringList parts = QString(mtab.readLine()).split(' ');
            if (parts.size()>=2 && parts.at(1)==mp) {
                return true;
            }
        }
    }
    #endif
    return false;
}

double RemoteFsDevice::usedCapacity()
{
    if (!isConnected() || !details.isLocalFile()) {
        return -1.0;
    }

    spaceInfo.setPath(mountPoint(details, false));
    return spaceInfo.size()>0 ? (spaceInfo.used()*1.0)/(spaceInfo.size()*1.0) : -1.0;
}

QString RemoteFsDevice::capacityString()
{
    if (!isConnected()) {
        return i18n("Not Connected");
    }

    if (!details.isLocalFile()) {
        return i18n("Capacity Unknown");
    }

    spaceInfo.setPath(mountPoint(details, false));
    return i18n("%1 free").arg(Utils::formatByteSize(spaceInfo.size()-spaceInfo.used()));
}

qint64 RemoteFsDevice::freeSpace()
{
    if (!isConnected() || !details.isLocalFile()) {
        return 0;
    }

    spaceInfo.setPath(mountPoint(details, false));
    return spaceInfo.size()-spaceInfo.used();
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
    details.url=MPDParseUtils::fixPath(details.url);
    #ifndef ENABLE_KDE_SUPPORT
    QSettings cfg;
    #endif
    if (HAS_GROUP(key)) {
        #ifdef ENABLE_KDE_SUPPORT
        KConfigGroup cfg(KGlobal::config(), key);
        #else
        cfg.beginGroup(key);
        #endif
        opts.useCache=GET_BOOL("useCache", true);
        coverFileName=GET_STRING("coverFileName", "cover.jpg");
        configured=true;
    } else {
        opts.useCache=true;
        coverFileName=QLatin1String("cover.jpg");
        configured=false;
    }
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
    connect(dlg, SIGNAL(updatedSettings(const QString &, const DeviceOptions &, RemoteFsDevice::Details)),
            SLOT(saveProperties(const QString &, const DeviceOptions &, RemoteFsDevice::Details)));
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

void RemoteFsDevice::saveProperties(const QString &newCoverFileName, const DeviceOptions &newOpts, Details newDetails)
{
    if (configured && opts==newOpts && newCoverFileName==coverFileName && details==newDetails) {
        return;
    }

    configured=true;
    Details oldDetails=details;
    newDetails.url=MPDParseUtils::fixPath(newDetails.url);

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
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), key);
    #else
    QSettings cfg;
    cfg.beginGroup(key);
    #endif
    SET_VALUE("useCache", opts.useCache);
    SET_VALUE("coverFileName", coverFileName);

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
