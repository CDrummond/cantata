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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "remotedevice.h"
#include "mpdparseutils.h"
#include "remotedevicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "actiondialog.h"
#include "network.h"
#include "httpserver.h"
#include <QtCore/QTimer>
#include <QtCore/QProcess>
#include <KDE/KUrl>
#include <KDE/KDiskFreeSpaceInfo>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KConfigGroup>
#include <KDE/KStandardDirs>
#include <kmountpoint.h>
#include <kde_file.h>
#include <stdio.h>
#include <mntent.h>

QString RemoteDevice::Details::mountPoint(bool create) const
{
    if (Prot_File==protocol) {
        return folder;
    }
    return Network::cacheDir("mount/"+name, create);
}

void RemoteDevice::Details::load(const QString &group)
{
    KConfigGroup grp(KGlobal::config(), group);

    protocol=(Protocol)grp.readEntry("protocol", (int)protocol);
    name=grp.readEntry("name", name);
    if (Prot_File!=protocol) {
        host=grp.readEntry("host", host);
        user=grp.readEntry("user", user);
        port=grp.readEntry("port", (int)port);
    }
    folder=grp.readEntry("folder", folder);
}

void RemoteDevice::Details::save(const QString &group) const
{
    KConfigGroup grp(KGlobal::config(), group);
    if (Prot_File==protocol) {
        grp.deleteEntry("host");
        grp.deleteEntry("user");
        grp.deleteEntry("port");
    } else {
        grp.writeEntry("host", host);
        grp.writeEntry("user", user);
        grp.writeEntry("port", (int)port);
    }
    grp.writeEntry("name", name);
    grp.writeEntry("folder", folder);
    grp.writeEntry("protocol", (int)protocol);
    grp.sync();
}

static const QLatin1String constCfgPrefix("RemoteDevice-");
static const QLatin1String constCfgKey("remoteDevices");

QList<Device *> RemoteDevice::loadAll(DevicesModel *m)
{
    QList<Device *> devices;
    KConfigGroup grp(KGlobal::config(), "General");
    QStringList names=grp.readEntry(constCfgKey, QStringList());
    foreach (const QString &n, names) {
        Details d;
        d.load(constCfgPrefix+n);
        if (d.isEmpty() || d.name!=n) {
            KGlobal::config()->deleteGroup(constCfgPrefix+n);
        } else {
            devices.append(new RemoteDevice(m, d));
        }
    }
    if (devices.count()!=names.count()) {
        KGlobal::config()->sync();
    }
    return devices;
}

RemoteDevice * RemoteDevice::create(DevicesModel *m, const QString &audio, const QString &cover, const Options &options, const Details &d)
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
    return new RemoteDevice(m, audio, cover, options, d);
}

void RemoteDevice::remove(RemoteDevice *dev)
{
    KConfigGroup grp(KGlobal::config(), "General");
    QStringList names=grp.readEntry(constCfgKey, QStringList());
    if (names.contains(dev->details.name)) {
        names.removeAll(dev->details.name);
        KGlobal::config()->deleteGroup(dev->udi());
        grp.writeEntry(constCfgKey, names);
        KGlobal::config()->sync();
    }
    if (dev->isConnected()) {
        dev->unmount();
    }
}

QString RemoteDevice::createUdi(const QString &n)
{
    return constCfgPrefix+n;
}

RemoteDevice::RemoteDevice(DevicesModel *m, const QString &audio, const QString &cover, const Options &options, const Details &d)
    : FsDevice(m, d.name)
    , lastCheck(0)
    , details(d)
    , proc(0)
//     , audioFolderSetting(audio)
{
    coverFileName=cover;
    opts=options;
    details.folder=MPDParseUtils::fixPath(details.folder);
    load();
    mount();
}

RemoteDevice::RemoteDevice(DevicesModel *m, const Details &d)
    : FsDevice(m, d.name)
    , lastCheck(0)
    , details(d)
    , proc(0)
{
    setup();
}

RemoteDevice::~RemoteDevice() {
    stopScanner();
}

void RemoteDevice::toggle()
{
    if (isConnected()) {
        unmount();
    } else {
        mount();
    }
}

void RemoteDevice::mount()
{
    if (Prot_File==details.protocol) {
        return;
    }
    if (isConnected() || proc) {
        return;
    }

    QString cmd;
    QStringList args;
    switch (details.protocol) {
    case Prot_Sshfs:
        if (!details.isEmpty()) {
            cmd=KStandardDirs::findExe("sshfs");
            if (!cmd.isEmpty()) {
                if (!QDir(details.mountPoint(true)).entryList(QDir::NoDotDot|QDir::AllEntries|QDir::Hidden).isEmpty()) {
                    emit error(i18n("Mount point (\"%1\") is not empty!", details.mountPoint(true)));
                    return;
                }
                args << details.user+QChar('@')+details.host+QChar(':')+details.folder << QLatin1String("-p")
                     << QString::number(details.port) << details.mountPoint(true)
                     << QLatin1String("-o") << QLatin1String("ServerAliveInterval=15");
            } else {
                emit error(i18n("\"sshfs\" is not installed!"));
            }
        }
        break;
    default:
        break;
    }

    if (!cmd.isEmpty()) {
        setStatusMessage(i18n("Connecting..."));
        proc=new QProcess(this);
        proc->setProperty("mount", true);
        connect(proc, SIGNAL(finished(int)), SLOT(procFinished(int)));
        proc->start(cmd, args, QIODevice::ReadOnly);
    }
}

void RemoteDevice::unmount()
{
    if (Prot_File==details.protocol) {
        return;
    }

    if (!isConnected() || proc || Prot_File==details.protocol) {
        return;
    }

    QString cmd;
    QStringList args;
    switch (details.protocol) {
    case Prot_Sshfs: {
        QString mp=details.mountPoint(false);
        if (!mp.isEmpty()) {
            cmd=KStandardDirs::findExe("fusermount");
            if (!cmd.isEmpty()) {
                args << QLatin1String("-u") << QLatin1String("-z") << mp;
            } else {
                emit error(i18n("\"fusermount\" is not installed!"));
            }
        }
        break;
    }
    default:
        break;
    }

    if (!cmd.isEmpty()) {
        setStatusMessage(i18n("Disconnecting..."));
        proc=new QProcess(this);
        proc->setProperty("unmount", true);
        connect(proc, SIGNAL(finished(int)), SLOT(procFinished(int)));
        proc->start(cmd, args, QIODevice::ReadOnly);
    }
}

void RemoteDevice::procFinished(int exitCode)
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

bool RemoteDevice::isConnected() const
{
    if (Prot_File==details.protocol) {
        return QDir(details.folder).exists();
    }

    QString mountPoint=details.mountPoint(false);
    if (mountPoint.isEmpty()) {
        return false;
    }

    if (opts.useCache && !audioFolder.isEmpty() && QFile::exists(cacheFileName())) {
        return true;
    }

    if (mountPoint.endsWith('/')) {
        mountPoint=mountPoint.left(mountPoint.length()-1);
    }
    KMountPoint::List list=KMountPoint::currentMountPoints();
    foreach (KMountPoint::Ptr p, list) {
        if (p->mountPoint()==mountPoint) {
            return true;
        }
    }
    return false;
}

double RemoteDevice::usedCapacity()
{
    if (!isConnected() || Prot_Sshfs==details.protocol) {
        return -1.0;
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(details.mountPoint(false));
    return inf.size()>0 ? (inf.used()*1.0)/(inf.size()*1.0) : -1.0;
}

QString RemoteDevice::capacityString()
{
    if (!isConnected()) {
        return i18n("Not Connected");
    }

    if (Prot_Sshfs==details.protocol) {
        return i18n("Capacity Unknown");
    }
    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(details.mountPoint(false));
    return i18n("%1 free", KGlobal::locale()->formatByteSize(inf.size()-inf.used()), 1);
}

qint64 RemoteDevice::freeSpace()
{
    if (!isConnected() || Prot_Sshfs==details.protocol) {
        return 0;
    }

    KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(details.mountPoint(false));
    return inf.size()-inf.used();
}

void RemoteDevice::load()
{
    if (isConnected()) {
        setAudioFolder();
        if (opts.useCache) {
            MusicLibraryItemRoot *root=new MusicLibraryItemRoot;
            if (root->fromXML(cacheFileName(), audioFolder)) {
                update=root;
                QTimer::singleShot(0, this, SLOT(cacheRead()));
                return;
            }
            delete root;
        }
        rescan();
    }
}

void RemoteDevice::renamed(const QString &oldName)
{
    KConfigGroup grp(KGlobal::config(), "General");
    QStringList names=grp.readEntry(constCfgKey, QStringList());
    if (names.contains(oldName)) {
        names.removeAll(oldName);
        KGlobal::config()->deleteGroup(createUdi(oldName));
    }
    if (!names.contains(details.name)) {
        names.append(details.name);
    }
    grp.writeEntry(constCfgKey, names);
    KGlobal::config()->sync();
}

void RemoteDevice::setup()
{
    QString key=udi();
    opts.load(key);
    details.load(key);
    details.folder=MPDParseUtils::fixPath(details.folder);
    KConfigGroup grp(KGlobal::config(), key);
    opts.useCache=grp.readEntry("useCache", true);
    coverFileName=grp.readEntry("coverFileName", "cover.jpg");
//     audioFolderSetting=grp.readEntry("audioFolder", "Music/");
    configured=KGlobal::config()->hasGroup(key);
    load();
}

void RemoteDevice::setAudioFolder()
{
//     KUrl url = KUrl(details.mountPoint(true));
//     url.addPath(audioFolderSetting);
//     url.cleanPath();
//     audioFolder=url.toLocalFile();
    audioFolder=details.mountPoint(true);
    if (!audioFolder.endsWith('/')) {
        audioFolder+='/';
    }
}

void RemoteDevice::configure(QWidget *parent)
{
    if (isRefreshing()) {
        return;
    }

    RemoteDevicePropertiesDialog *dlg=new RemoteDevicePropertiesDialog(parent);
    connect(dlg, SIGNAL(updatedSettings(const QString &, const QString &, const Device::Options &, const RemoteDevice::Details &)),
            SLOT(saveProperties(const QString &, const QString &, const Device::Options &, const RemoteDevice::Details &)));
    if (!configured) {
        connect(dlg, SIGNAL(cancelled()), SLOT(saveProperties()));
    }
    dlg->show(QString(), coverFileName, opts, details,
              DevicePropertiesWidget::Prop_All-DevicePropertiesWidget::Prop_Folder);
}

bool RemoteDevice::canPlaySongs() const
{
    return Prot_File==details.protocol || HttpServer::self()->isAlive();
}

static inline QString toString(bool b)
{
    return b ? QLatin1String("true") : QLatin1String("false");
}

void RemoteDevice::saveOptions()
{
    opts.save(udi());
}

void RemoteDevice::saveProperties()
{
    saveProperties(QString(), coverFileName, opts, details);
}

void RemoteDevice::saveProperties(const QString &newPath, const QString &newCoverFileName, const Device::Options &newOpts, const Details &newDetails)
{
    Q_UNUSED(newPath);

//     QString nPath=MPDParseUtils::fixPath(newPath);
    if (configured && opts==newOpts && /*nPath==audioFolderSetting && */newCoverFileName==coverFileName && details==newDetails) {
        return;
    }

    configured=true;
    bool diffCacheSettings=opts.useCache!=newOpts.useCache;
//     QString oldPath=audioFolderSetting;
    QString oldName=details.name;
    QString oldFolder=details.folder;
    opts=newOpts;
    details=newDetails;
    details.folder=MPDParseUtils::fixPath(details.folder);
    if (diffCacheSettings) {
        if (opts.useCache) {
            saveCache();
        } else {
            removeCache();
        }
    }
    coverFileName=newCoverFileName;
    QString key=udi();
    details.save(key);
//     audioFolderSetting=newPath;
    KConfigGroup grp(KGlobal::config(), key);
    grp.writeEntry("useCache", opts.useCache);
    grp.writeEntry("coverFileName", coverFileName);
//     grp.writeEntry("audioFolder", audioFolderSetting);

    if (!oldName.isEmpty() && oldName!=details.name) {
        renamed(oldName);
        emit udiChanged(createUdi(oldName), createUdi(details.name));
        m_itemData=details.name;
        setStatusMessage(QString());
        if (isConnected()) {
            unmount();
        }
    } else if (oldFolder!=details.folder) {
        if (isConnected()) {
            removeCache();
            unmount();
        }
    } /*else if (oldPath!=audioFolderSetting) {
        if (isConnected()) {
            setAudioFolder();
        }
        rescan();
    }*/
}
