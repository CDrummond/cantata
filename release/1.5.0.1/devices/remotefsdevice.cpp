/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "support/inputdialog.h"
#include "support/utils.h"
#include "http/httpserver.h"
#include "support/localize.h"
#include "support/configuration.h"
#include "mountpoints.h"
#include "mounterinterface.h"
#include "avahi/avahi.h"
#include "avahi/avahiservice.h"
#include <QDBusConnection>
#include <QTimer>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <stdio.h>
#include <unistd.h>

const QLatin1String RemoteFsDevice::constPromptPassword("-");
const QLatin1String RemoteFsDevice::constSshfsProtocol("sshfs");
const QLatin1String RemoteFsDevice::constFileProtocol("file");
const QLatin1String RemoteFsDevice::constDomainQuery("domain");
const QLatin1String RemoteFsDevice::constSambaProtocol("smb");
const QLatin1String RemoteFsDevice::constServiceNameQuery("name");
const QLatin1String RemoteFsDevice::constSambaAvahiProtocol("smb-avahi");

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
    Configuration cfg(constCfgPrefix+group);
    name=group;
    QString u=cfg.get("url", QString());

    if (u.isEmpty()) {
        QString protocol=cfg.get("protocol", QString());
        QString host=cfg.get("host", QString());
        QString user=cfg.get("user", QString());
        QString path=cfg.get("path", QString());
        int port=cfg.get("port", 0);
        u=(protocol.isEmpty() ? "file" : protocol+"://")+user+(!user.isEmpty() && !host.isEmpty() ? "@" : "")+host+(0==port ? "" : (":"+QString::number(port)))+path;
    }
    if (!u.isEmpty()) {
        url=QUrl(u);
    }
    extraOptions=cfg.get("extraOptions", QString());
    configured=cfg.get("configured", configured);
}

void RemoteFsDevice::Details::save() const
{
    Configuration cfg(constCfgPrefix+name);

    cfg.set("url", url.toString());
    cfg.set("extraOptions", extraOptions);
    cfg.set("configured", configured);
}

static inline bool isValid(const RemoteFsDevice::Details &d)
{
    return d.isLocalFile() || RemoteFsDevice::constSshfsProtocol==d.url.scheme() ||
           RemoteFsDevice::constSambaProtocol==d.url.scheme() || RemoteFsDevice::constSambaAvahiProtocol==d.url.scheme();
}

static inline bool isMountable(const RemoteFsDevice::Details &d)
{
    return RemoteFsDevice::constSshfsProtocol==d.url.scheme() ||
           RemoteFsDevice::constSambaProtocol==d.url.scheme() || RemoteFsDevice::constSambaAvahiProtocol==d.url.scheme();
}

QList<Device *> RemoteFsDevice::loadAll(MusicModel *m)
{
    QList<Device *> devices;
    Configuration cfg;
    QStringList names=cfg.get(constCfgKey, QStringList());
    foreach (const QString &n, names) {
        Details d;
        d.load(n);
        if (d.isEmpty()) {
            cfg.removeGroup(constCfgPrefix+n);
        } else if (isValid(d)) {
            devices.append(new RemoteFsDevice(m, d));
        }
    }
    return devices;
}

Device * RemoteFsDevice::create(MusicModel *m, const DeviceOptions &options, const Details &d)
{
    if (d.isEmpty()) {
        return 0;
    }
    Configuration cfg;
    QStringList names=cfg.get(constCfgKey, QStringList());
    if (names.contains(d.name)) {
        return 0;
    }
    names.append(d.name);
    cfg.set(constCfgKey, names);
    d.save();
    if (isValid(d)) {
        return new RemoteFsDevice(m, options, d);
    }
    return 0;
}

void RemoteFsDevice::destroy(bool removeFromConfig)
{
    if (removeFromConfig) {
        Configuration cfg;
        QStringList names=cfg.get(constCfgKey, QStringList());
        if (names.contains(details.name)) {
            names.removeAll(details.name);
            cfg.removeGroup(id());
            cfg.set(constCfgKey, names);
        }
    }
    stopScanner();
    if (isConnected()) {
        unmount();
    }
    if (isMountable(details)) {
        QString mp=mountPoint(details, false);
        if (!mp.isEmpty()) {
            QDir d(mp);
            if (d.exists()) {
                d.rmdir(mp);
            }
        }
    }
    deleteLater();
}

void RemoteFsDevice::serviceAdded(const QString &name)
{
    if (name==details.serviceName && constSambaAvahiProtocol==details.url.scheme()) {
        sub=i18n("Available");
        updateStatus();
    }
}

void RemoteFsDevice::serviceRemoved(const QString &name)
{
    if (name==details.serviceName && constSambaAvahiProtocol==details.url.scheme()) {
        sub=i18n("Not Available");
        updateStatus();
    }
}

QString RemoteFsDevice::createUdi(const QString &n)
{
    return constCfgPrefix+n;
}

void RemoteFsDevice::renamed(const QString &oldName, const QString &newName)
{
    Configuration cfg;
    QStringList names=cfg.get(constCfgKey, QStringList());
    if (names.contains(oldName)) {
        names.removeAll(oldName);
        cfg.removeGroup(createUdi(oldName));
    }
    if (!names.contains(newName)) {
        names.append(newName);
    }
    cfg.set(constCfgKey, names);
}

RemoteFsDevice::RemoteFsDevice(MusicModel *m, const DeviceOptions &options, const Details &d)
    : FsDevice(m, d.name, createUdi(d.name))
    , mountToken(0)
    , currentMountStatus(false)
    , details(d)
    , proc(0)
    , mounterIface(0)
    , messageSent(false)
{
    opts=options;
//    details.path=Utils::fixPath(details.path);
    load();
    mount();
    icn=Icon(details.isLocalFile() ? "inode-directory" : (constSshfsProtocol==details.url.scheme() ? "utilities-terminal" : "network-server"));
}

RemoteFsDevice::RemoteFsDevice(MusicModel *m, const Details &d)
    : FsDevice(m, d.name, createUdi(d.name))
    , mountToken(0)
    , currentMountStatus(false)
    , details(d)
    , proc(0)
    , mounterIface(0)
    , messageSent(false)
{
//    details.path=Utils::fixPath(details.path);
    setup();
    icn=Icon(details.isLocalFile() ? "inode-directory" : (constSshfsProtocol==details.url.scheme() ? "utilities-terminal" : "network-server"));
}

RemoteFsDevice::~RemoteFsDevice() {
}

void RemoteFsDevice::toggle()
{
    if (isConnected()) {
        stopScanner();
        unmount();
    } else {
        mount();
    }
}

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

void RemoteFsDevice::mount()
{
    if (details.isLocalFile()) {
        return;
    }
    if (isConnected() || proc) {
        return;
    }

    if (messageSent) {
        return;
    }
    if (constSambaAvahiProtocol==details.url.scheme()) {
        Details det=details;
        AvahiService *srv=Avahi::self()->getService(det.serviceName);
        if (!srv || srv->getHost().isEmpty() || 0==srv->getPort()) {
            emit error(i18n("Failed to resolve connection details for %1", details.name));
            return;
        }
        if (constPromptPassword==det.url.password()) {
            bool ok=false;
            QString passwd=InputDialog::getPassword(QString(), &ok, QApplication::activeWindow());
            if (!ok) {
                return;
            }
            det.url.setPassword(passwd);
        }
        det.url.setScheme(constSambaProtocol);
        det.url.setHost(srv->getHost());
        det.url.setPort(srv->getPort());
        mounter()->mount(det.url.toString(), mountPoint(details, true), getuid(), getgid(), getpid());
        setStatusMessage(i18n("Connecting..."));
        messageSent=true;
        return;
    }
    if (constSambaProtocol==details.url.scheme()) {
        Details det=details;
        if (constPromptPassword==det.url.password()) {
            bool ok=false;
            QString passwd=InputDialog::getPassword(QString(), &ok, QApplication::activeWindow());
            if (!ok) {
                return;
            }
            det.url.setPassword(passwd);
        }
        mounter()->mount(det.url.toString(), mountPoint(details, true), getuid(), getgid(), getpid());
        setStatusMessage(i18n("Connecting..."));
        messageSent=true;
        return;
    }

    QString cmd;
    QStringList args;
    if (!details.isLocalFile() && !details.isEmpty()) {
        // If user has added 'IdentityFile' to extra options, then no password prompting is required...
        bool needAskPass=!details.extraOptions.contains("IdentityFile=");

        if (needAskPass) {
            if (ttyname(0)) {
                emit error(i18n("Password prompting does not work when cantata is started from the commandline."));
                return;
            }
            QStringList askPassList;
            if (Utils::KDE==Utils::currentDe()) {
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
        }
        cmd=Utils::findExe("sshfs");
        if (!cmd.isEmpty()) {
            if (!QDir(mountPoint(details, true)).entryList(QDir::NoDot|QDir::NoDotDot|QDir::AllEntries|QDir::Hidden).isEmpty()) {
                emit error(i18n("Mount point (\"%1\") is not empty!", mountPoint(details, true)));
                return;
            }

            args << details.url.userName()+QChar('@')+details.url.host()+QChar(':')+details.url.path()<< QLatin1String("-p")
                 << QString::number(details.url.port()) << mountPoint(details, true)
                 << QLatin1String("-o") << QLatin1String("ServerAliveInterval=15");
            //<< QLatin1String("-o") << QLatin1String("Ciphers=arcfour");
            if (!details.extraOptions.isEmpty()) {
                args << details.extraOptions.split(' ', QString::SkipEmptyParts);
            }
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

    if (messageSent) {
        return;
    }
    if (constSambaProtocol==details.url.scheme() || constSambaAvahiProtocol==details.url.scheme()) {
        mounter()->umount(mountPoint(details, false), getpid());
        setStatusMessage(i18n("Disconnecting..."));
        messageSent=true;
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
        emit error(wasMount ? i18n("Failed to connect to \"%1\"", details.name)
                            : i18n("Failed to disconnect from \"%1\"", details.name));
        setStatusMessage(QString());
    } else if (wasMount) {
        setStatusMessage(i18n("Updating tracks..."));
        load();
        emit connectionStateHasChanged(id(), true);
    } else {
        setStatusMessage(QString());
        update=new MusicLibraryItemRoot;
        scanned=false;
        emit updating(id(), false);
        emit connectionStateHasChanged(id(), false);
    }
}

void RemoteFsDevice::mountStatus(const QString &mp, int pid, int st)
{
    if (pid==getpid() && mp==mountPoint(details, false)) {
        messageSent=false;
        if (0!=st) {
            emit error(i18n("Failed to connect to \"%1\"", details.name));
            setStatusMessage(QString());
        } else {
            setStatusMessage(i18n("Updating tracks..."));
            load();
            emit connectionStateHasChanged(id(), true);
        }
    }
}

void RemoteFsDevice::umountStatus(const QString &mp, int pid, int st)
{
    if (pid==getpid() && mp==mountPoint(details, false)) {
        messageSent=false;
        if (0!=st) {
            emit error(i18n("Failed to disconnect from \"%1\"", details.name));
            setStatusMessage(QString());
        } else {
            setStatusMessage(QString());
            update=new MusicLibraryItemRoot;
            emit updating(id(), false);
            emit connectionStateHasChanged(id(), false);
        }
    }
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
    if (cacheProgress>-1) {
        return (cacheProgress*1.0)/100.0;
    }
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
    if (cacheProgress>-1) {
        return statusMessage();
    }
    if (!isConnected()) {
        return i18n("Not Connected");
    }

    spaceInfo.setPath(mountPoint(details, false));
    if (isOldSshfs()) {
        return i18n("Capacity Unknown");
    }
    return i18n("%1 free", Utils::formatByteSize(spaceInfo.size()-spaceInfo.used()));
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
    if (RemoteFsDevice::constSambaAvahiProtocol==details.url.scheme()) {
        // Start Avahi listener...
        Avahi::self();
        #if QT_VERSION < 0x050000
        if (details.url.hasQueryItem(constServiceNameQuery)) {
            details.serviceName=details.url.queryItemValue(constServiceNameQuery);
        }
        #else // QT_VERSION
        QUrlQuery q(details.url);
        if (q.hasQueryItem(constServiceNameQuery)) {
            details.serviceName=q.queryItemValue(constServiceNameQuery);
        }
        #endif // QT_VERSION

        if (!details.serviceName.isEmpty()) {
            AvahiService *srv=Avahi::self()->getService(details.serviceName);
            if (!srv || srv->getHost().isEmpty()) {
                sub=i18n("Not Available");
            } else {
                sub=i18n("Available");
            }
        }
        connect(Avahi::self(), SIGNAL(serviceAdded(QString)), SLOT(serviceAdded(QString)));
        connect(Avahi::self(), SIGNAL(serviceRemoved(QString)), SLOT(serviceRemoved(QString)));
    }

    if (isConnected()) {
        setAudioFolder();
        readOpts(settingsFileName(), opts, true);
        rescan(false); // Read from cache if we have it!
    }
}

void RemoteFsDevice::setup()
{
    details.load(details.name);
    configured=details.configured;
    if (isConnected()) {
        readOpts(settingsFileName(), opts, true);
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
              DevicePropertiesWidget::Prop_All-(DevicePropertiesWidget::Prop_Name+DevicePropertiesWidget::Prop_Folder+DevicePropertiesWidget::Prop_AutoScan),
              0, false, isConnected());
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
    opts.save(id());
}

void RemoteFsDevice::saveProperties()
{
    saveProperties(opts, details);
}

void RemoteFsDevice::saveProperties(const DeviceOptions &newOpts, const Details &nd)
{
    bool connected=isConnected();
    if (configured && (!connected || opts==newOpts) && (connected || details==nd)) {
        return;
    }

    bool isLocal=details.isLocalFile();

    if (connected) {
        if (!configured) {
            details.configured=configured=true;
            details.save();
        }
        if (opts.useCache!=newOpts.useCache) {
            if (opts.useCache) {
                saveCache();
            } else if (opts.useCache && !newOpts.useCache) {
                removeCache();
            }
        }
        opts=newOpts;
        writeOpts(settingsFileName(), opts, true);
    }
    if (!connected || isLocal) {
        Details newDetails=nd;
        Details oldDetails=details;
        bool newName=!oldDetails.name.isEmpty() && oldDetails.name!=newDetails.name;
        bool newDir=oldDetails.url.path()!=newDetails.url.path();

        if (isLocal && newDir && opts.useCache) {
            removeCache();
        }
        details=newDetails;
        details.configured=configured=true;
        details.save();

        if (newName) {
            if (!details.isLocalFile()) {
                QString oldMount=mountPoint(oldDetails, false);
                if (!oldMount.isEmpty() && QDir(oldMount).exists()) {
                    ::rmdir(QFile::encodeName(oldMount).constData());
                }
            }
            setData(details.name);
            renamed(oldDetails.name, details.name);
            deviceId=createUdi(details.name);
            emit udiChanged();
            m_itemData=details.name;
            setStatusMessage(QString());
        }
        if (isLocal && newDir && scanned) {
            rescan(true);
        }
    }
    emit configurationChanged();
}

QString RemoteFsDevice::settingsFileName() const
{
    if (audioFolder.isEmpty()) {
        setAudioFolder();
    }
    return audioFolder+constCantataSettingsFile;
}
