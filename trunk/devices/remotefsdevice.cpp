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
#include "remotedevicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "actiondialog.h"
#include "utils.h"
#include "httpserver.h"
#include "localize.h"
#include "settings.h"
#include "mountpoints.h"
#ifdef ENABLE_MOUNTER
#include "mounterinterface.h"
#include <QtDBus/QDBusConnection>
#endif
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

const QLatin1String RemoteFsDevice::constSshfsProtocol("sshfs");
const QLatin1String RemoteFsDevice::constFileProtocol("file");
#ifdef ENABLE_MOUNTER
const QLatin1String RemoteFsDevice::constDomainQuery("domain");
const QLatin1String RemoteFsDevice::constSambaProtocol("smb");
#endif
static const QLatin1String constCfgPrefix("RemoteFsDevice-");
static const QLatin1String constCfgKey("remoteFsDevices");

static QString mountPoint(const RemoteFsDevice::Details &details, bool create)
{
    if (details.isLocalFile()) {
        return details.url.path();
    }
    return Utils::cacheDir(QLatin1String("mount/")+details.name, create);
}

void RemoteFsDevice::Details::load(const QString &group)
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), constCfgPrefix+group);
    #else
    QSettings cfg;
    cfg.beginGroup(constCfgPrefix+group);
    #endif
    name=group;
    QString u=GET_STRING("url", QString());

    if (u.isEmpty()) {
        QString protocol=GET_STRING("protocol", QString());
        QString host=GET_STRING("host", QString());
        QString user=GET_STRING("user", QString());
        QString path=GET_STRING("path", QString());
        int port=GET_INT("port", 0);
        u=(protocol.isEmpty() ? "file" : protocol+"://")+user+(!user.isEmpty() && !host.isEmpty() ? "@" : "")+host+(0==port ? "" : (":"+QString::number(port)))+path;
    }
    if (!u.isEmpty()) {
        url=QUrl(u);
    }
}

void RemoteFsDevice::Details::save() const
{
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), constCfgPrefix+name);
    #else
    QSettings cfg;
    cfg.beginGroup(constCfgPrefix+name);
    #endif
    SET_VALUE("url", url.toString());
    CFG_SYNC;
}

static inline bool isValid(const RemoteFsDevice::Details &d)
{
    return d.isLocalFile() || RemoteFsDevice::constSshfsProtocol==d.url.scheme()
        #ifdef ENABLE_MOUNTER
        || RemoteFsDevice::constSambaProtocol==d.url.scheme()
        #endif
        ;
}

static inline bool isMountable(const RemoteFsDevice::Details &d)
{
    return RemoteFsDevice::constSshfsProtocol==d.url.scheme()
        #ifdef ENABLE_MOUNTER
        || RemoteFsDevice::constSambaProtocol==d.url.scheme()
        #endif
        ;
}

QList<Device *> RemoteFsDevice::loadAll(DevicesModel *m)
{
    QList<Device *> devices;
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), "General");
    #else
    QSettings cfg;
    #endif
    QStringList names=GET_STRINGLIST(constCfgKey, QStringList());
    foreach (const QString &n, names) {
        Details d;
        d.load(n);
        if (d.isEmpty()) {
            REMOVE_GROUP(constCfgPrefix+n);
        } else if (isValid(d)) {
            devices.append(new RemoteFsDevice(m, d));
        }
    }
    if (devices.count()!=names.count()) {
        CFG_SYNC;
    }
    return devices;
}

Device * RemoteFsDevice::create(DevicesModel *m, const DeviceOptions &options, const Details &d)
{
    if (d.isEmpty()) {
        return 0;
    }
    #ifdef ENABLE_KDE_SUPPORT
    KConfigGroup cfg(KGlobal::config(), "General");
    #else
    QSettings cfg;
    #endif
    QStringList names=GET_STRINGLIST(constCfgKey, QStringList());
    if (names.contains(d.name)) {
        return 0;
    }
    names.append(d.name);
    SET_VALUE(constCfgKey, names);
    d.save();
    if (isValid(d)) {
        return new RemoteFsDevice(m, options, d);
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
        if (isMountable(rfs->details)) {
            QString mp=mountPoint(rfs->details, false);
            if (!mp.isEmpty()) {
                QDir d(mp);
                if (d.exists()) {
                    d.rmdir(mp);
                }
            }
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

RemoteFsDevice::RemoteFsDevice(DevicesModel *m, const DeviceOptions &options, const Details &d)
    : FsDevice(m, d.name, createUdi(d.name))
    , mountToken(0)
    , currentMountStatus(false)
    , details(d)
    , proc(0)
    #ifdef ENABLE_MOUNTER
    , mounterIface(0)
    #endif
{
    opts=options;
//    details.path=Utils::fixPath(details.path);
    load();
    mount();
}

RemoteFsDevice::RemoteFsDevice(DevicesModel *m, const Details &d)
    : FsDevice(m, d.name, createUdi(d.name))
    , mountToken(0)
    , currentMountStatus(false)
    , details(d)
    , proc(0)
    #ifdef ENABLE_MOUNTER
    , mounterIface(0)
    , messageSent(false)
    #endif
{
//    details.path=Utils::fixPath(details.path);
    setup();
}

RemoteFsDevice::~RemoteFsDevice() {
    stopScanner(false);
}

void RemoteFsDevice::toggle()
{
    if (isConnected()) {
        if (scanner) {
            scanner->stop();
        }
        unmount();
    } else {
        mount();
    }
}

#ifdef ENABLE_MOUNTER
ComGooglecodeCantataMounterInterface * RemoteFsDevice::mounter()
{
    if (!mounterIface) {
        if (!QDBusConnection::systemBus().interface()->isServiceRegistered(ComGooglecodeCantataMounterInterface::staticInterfaceName())) {
            QDBusConnection::systemBus().interface()->startService(ComGooglecodeCantataMounterInterface::staticInterfaceName());
        }
        mounterIface=new ComGooglecodeCantataMounterInterface(ComGooglecodeCantataMounterInterface::staticInterfaceName(),
                                                              "/Mounter", QDBusConnection::systemBus(), this);
        connect(mounterIface, SIGNAL(mountStatus(const QString &, int, int)), SLOT(mountStatus(const QString &, int, int)));
        connect(mounterIface, SIGNAL(umountStatus(const QString &, int, int)), SLOT(umountStatus(const QString &, int, int)));
    }
    return mounterIface;
}
#endif

void RemoteFsDevice::mount()
{
    if (details.isLocalFile()) {
        return;
    }
    if (isConnected() || proc) {
        return;
    }

    #ifdef ENABLE_MOUNTER
    if (messageSent) {
        return;
    }
    if (constSambaProtocol==details.url.scheme()) {
        mounter()->mount(details.url.toString(), mountPoint(details, true), getuid(), getgid(), getpid());
        setStatusMessage(i18n("Connecting..."));
        messageSent=true;
        return;
    }
    #endif

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
            emit error(i18n("No suitable ssh-askpass application installed! This is required for entering passwords."));
            return;
        }
        cmd=Utils::findExe("sshfs");
        if (!cmd.isEmpty()) {
            if (!QDir(mountPoint(details, true)).entryList(QDir::NoDot|QDir::NoDotDot|QDir::AllEntries|QDir::Hidden).isEmpty()) {
                emit error(i18n("Mount point (\"%1\") is not empty!").arg(mountPoint(details, true)));
                return;
            }

            args << details.url.userName()+QChar('@')+details.url.host()+QChar(':')+details.url.path()<< QLatin1String("-p")
                 << QString::number(details.port) << mountPoint(details, true)
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

    #ifdef ENABLE_MOUNTER
    if (messageSent) {
        return;
    }
    if (constSambaProtocol==details.url.scheme()) {
        mounter()->umount(mountPoint(details, false), getpid());
        setStatusMessage(i18n("Disconnecting..."));
        messageSent=true;
        return;
    }
    #endif

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

void RemoteFsDevice::mountStatus(const QString &mp, int pid, int st)
{
    #ifdef ENABLE_MOUNTER
    if (pid==getpid() && mp==mountPoint(details, false)) {
        messageSent=false;
        if (0!=st) {
            emit error(i18n("Failed to connect to \"%1\"").arg(details.name));
            setStatusMessage(QString());
        } else {
            setStatusMessage(i18n("Updating tracks..."));
            load();
        }
    }
    #else
    Q_UNUSED(src)
    Q_UNUSED(pid)
    Q_UNUSED(st)
    #endif
}

void RemoteFsDevice::umountStatus(const QString &mp, int pid, int st)
{
    #ifdef ENABLE_MOUNTER
    if (pid==getpid() && mp==mountPoint(details, false)) {
        messageSent=false;
        if (0!=st) {
            emit error(i18n("Failed to disconnect from \"%1\"").arg(details.name));
            setStatusMessage(QString());
        } else {
            setStatusMessage(QString());
            update=new MusicLibraryItemRoot;
            emit updating(udi(), false);
        }
    }
    #else
    Q_UNUSED(src)
    Q_UNUSED(pid)
    Q_UNUSED(st)
    #endif
}

bool RemoteFsDevice::isConnected() const
{
    if (details.isLocalFile()) {
       if (QDir(details.url.path()).exists()) {
           return true;
       }
       clear();
       return false;
    }

    if (mountToken==MountPoints::self()->currentToken()) {
        return currentMountStatus;
    }
    QString mp=mountPoint(details, false);
    if (mp.isEmpty()) {
        clear();
        return false;
    }

    if (opts.useCache && !audioFolder.isEmpty() && QFile::exists(cacheFileName())) {
        currentMountStatus=true;
        return true;
    }

    mountToken=MountPoints::self()->currentToken();
    currentMountStatus=MountPoints::self()->isMounted(mp);
    if (currentMountStatus) {
        return true;
    }
    clear();
    return false;
}

bool RemoteFsDevice::isOldSshfs()
{
    return constSshfsProtocol==details.url.scheme() && 0==spaceInfo.used() && 1073741824000==spaceInfo.size();
}

double RemoteFsDevice::usedCapacity()
{
    if (!isConnected()) {
        return -1.0;
    }

    spaceInfo.setPath(mountPoint(details, false));
    if (isOldSshfs()) {
        return -1.0;
    }
    return spaceInfo.size()>0 ? (spaceInfo.used()*1.0)/(spaceInfo.size()*1.0) : -1.0;
}

QString RemoteFsDevice::capacityString()
{
    if (!isConnected()) {
        return i18n("Not Connected");
    }

    spaceInfo.setPath(mountPoint(details, false));
    if (isOldSshfs()) {
        return i18n("Capacity Unknown");
    }
    return i18n("%1 free").arg(Utils::formatByteSize(spaceInfo.size()-spaceInfo.used()));
}

qint64 RemoteFsDevice::freeSpace()
{
    if (!isConnected()) { // || !details.isLocalFile()) {
        return 0;
    }

    spaceInfo.setPath(mountPoint(details, false));
    if (isOldSshfs()) {
        return 0;
    }
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
    details.load(details.name);
//    details.path=Utils::fixPath(details.path);
    #ifndef ENABLE_KDE_SUPPORT
    QSettings cfg;
    #endif
    if (HAS_GROUP(key)) {
        opts.checkCoverSize();
        configured=true;
    } else {
        opts.useCache=true;
        opts.coverName=QLatin1String("cover.jpg");
        opts.coverMaxSize=0;
        configured=false;
    }
    load();
}

void RemoteFsDevice::setAudioFolder() const
{
    audioFolder=Utils::fixPath(mountPoint(details, true));
}

void RemoteFsDevice::configure(QWidget *parent)
{
    if (isRefreshing()) {
        return;
    }

    RemoteDevicePropertiesDialog *dlg=new RemoteDevicePropertiesDialog(parent);
    connect(dlg, SIGNAL(updatedSettings(const DeviceOptions &, const RemoteFsDevice::Details &)),
            SLOT(saveProperties(const DeviceOptions &, const RemoteFsDevice::Details &)));
    if (!configured) {
        connect(dlg, SIGNAL(cancelled()), SLOT(saveProperties()));
    }
    dlg->show(opts, details,
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
    saveProperties(opts, details);
}

void RemoteFsDevice::saveProperties(const DeviceOptions &newOpts, const Details &nd)
{
    if (configured && opts==newOpts && details==nd) {
        return;
    }

    configured=true;
    Details newDetails=nd;
    Details oldDetails=details;
//    newDetails.path=Utils::fixPath(newDetails.path);
    bool diffUrl=oldDetails.url!=newDetails.url;

    if (opts.useCache!=newOpts.useCache || diffUrl) { // Cache/url settings changed
        if (opts.useCache && !diffUrl) {
            saveCache();
        } else if (opts.useCache && !newOpts.useCache) {
            removeCache();
        }
    }

    // Name/or URL changed - need to unmount...
    bool newName=!oldDetails.name.isEmpty() && oldDetails.name!=newDetails.name;
    if ((newName || diffUrl) && isConnected()) {
        unmount();
    }

    opts=newOpts;
    details=newDetails;
    QString key=udi();
    details.save();
    opts.save(key);

    if (newName) {
        QString oldMount=mountPoint(oldDetails, false);
        if (!oldMount.isEmpty() && QDir(oldMount).exists()) {
            ::rmdir(QFile::encodeName(oldMount).constData());
        }
        setData(details.name);
        renamed(oldDetails.name, details.name);
        deviceId=createUdi(details.name);
        emit udiChanged();
        m_itemData=details.name;
        setStatusMessage(QString());
    }
}
