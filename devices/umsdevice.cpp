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

#include "umsdevice.h"
#include "utils.h"
#include "devicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "actiondialog.h"
#include "localize.h"
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QTextStream>

static const QLatin1String constSettingsFile("/.is_audio_player");
static const QLatin1String constMusicFolderKey("audio_folder");

UmsDevice::UmsDevice(DevicesModel *m, Solid::Device &dev)
    : FsDevice(m, dev)
    , access(dev.as<Solid::StorageAccess>())
{
    spaceInfo.setPath(access->filePath());
    setup();
}

UmsDevice::~UmsDevice()
{
}

void UmsDevice::connectionStateChanged()
{
    if (isConnected()) {
        spaceInfo.setPath(access->filePath());
        setup();
        if ((opts.autoScan || scanned) && (!opts.useCache || !readCache())){ // Only scan if we are set to auto scan, or we have already scanned before...
            rescan();
        } else if (!scanned && (!opts.useCache || !readCache())) { // Attempt to read cache, even if autoScan set to false
            setStatusMessage(i18n("Not Scanned"));
        }
    } else {
        clear();
    }
}

void UmsDevice::toggle()
{
    if (solidDev.isValid() && access && access->isValid()) {
        if (access->isAccessible()) {
            if (scanner) {
                scanner->stop();
            }
            access->teardown();
        } else {
            access->setup();
        }
    }
}

bool UmsDevice::isConnected() const
{
    return solidDev.isValid() && access && access->isValid() && access->isAccessible();
}

double UmsDevice::usedCapacity()
{
    if (!isConnected()) {
        return -1.0;
    }

    return spaceInfo.size()>0 ? (spaceInfo.used()*1.0)/(spaceInfo.size()*1.0) : -1.0;
}

QString UmsDevice::capacityString()
{
    if (!isConnected()) {
        return i18n("Not Connected");
    }

    return i18n("%1 free").arg(Utils::formatByteSize(spaceInfo.size()-spaceInfo.used()));
}

qint64 UmsDevice::freeSpace()
{
    if (!isConnected()) {
        return 0;
    }

    return spaceInfo.size()-spaceInfo.used();
}

void UmsDevice::setup()
{
    if (!isConnected()) {
        return;
    }

    QString path=spaceInfo.path();
    audioFolder = path;

    QFile file(path+constSettingsFile);
    QString audioFolderSetting;

    opts=DeviceOptions();
    if (file.open(QIODevice::ReadOnly|QIODevice::Text)) {
        configured=true;
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith(constMusicFolderKey+"=")) {
                audioFolderSetting=audioFolder=Utils::cleanPath(path+'/'+line.section('=', 1, 1));
                if (!QDir(audioFolder).exists()) {
                    audioFolder = path;
                }
            } else if (line.startsWith(constMusicFilenameSchemeKey+"=")) {
                QString scheme = line.section('=', 1, 1);
                //protect against empty setting.
                if( !scheme.isEmpty() ) {
                    opts.scheme = scheme;
                }
            } else if (line.startsWith(constVfatSafeKey+"="))  {
                opts.vfatSafe = QLatin1String("true")==line.section('=', 1, 1);
            } else if (line.startsWith(constAsciiOnlyKey+"=")) {
                opts.asciiOnly = QLatin1String("true")==line.section('=', 1, 1);
            } else if (line.startsWith(constIgnoreTheKey+"=")) {
                opts.ignoreThe = QLatin1String("true")==line.section('=', 1, 1);
            } else if (line.startsWith(constReplaceSpacesKey+"="))  {
                opts.replaceSpaces = QLatin1String("true")==line.section('=', 1, 1);
            } else {
                unusedParams+=line;
            }
        }
    }

    bool haveOpts=FsDevice::readOpts(path+constCantataSettingsFile, opts, false);
    if (!configured) {
        configured=haveOpts;
    }

    if (opts.coverName.isEmpty()) {
        opts.coverName=constDefCoverFileName;
    }

    // No setting, see if any standard dirs exist in path...
    if (audioFolderSetting.isEmpty() || audioFolderSetting!=audioFolder) {
        QStringList dirs;
        dirs << QLatin1String("Music") << QLatin1String("MUSIC")
             << QLatin1String("Albums") << QLatin1String("ALBUMS");

        foreach (const QString &d, dirs) {
            if (QDir(path+d).exists()) {
                audioFolder=path+d;
                break;
            }
        }
    }

    if (!audioFolder.endsWith('/')) {
        audioFolder+='/';
    }

    if ((opts.autoScan || scanned) && (!opts.useCache || !readCache())){ // Only scan if we are set to auto scan, or we have already scanned before...
        rescan();
    } else if (!scanned && (!opts.useCache || !readCache())) { // Attempt to read cache, even if autoScan set to false
        setStatusMessage(i18n("Not Scanned"));
    }
}

void UmsDevice::configure(QWidget *parent)
{
    if (!isIdle()) {
        return;
    }

    DevicePropertiesDialog *dlg=new DevicePropertiesDialog(parent);
    connect(dlg, SIGNAL(updatedSettings(const QString &, const DeviceOptions &)), SLOT(saveProperties(const QString &, const DeviceOptions &)));
    if (!configured) {
        connect(dlg, SIGNAL(cancelled()), SLOT(saveProperties()));
    }
    dlg->show(audioFolder, opts,
              qobject_cast<ActionDialog *>(parent) ? (DevicePropertiesWidget::Prop_All-DevicePropertiesWidget::Prop_Folder)
                                                   : DevicePropertiesWidget::Prop_All);
}

void UmsDevice::saveProperties()
{
    saveProperties(audioFolder, opts);
}

static inline QString toString(bool b)
{
    return b ? QLatin1String("true") : QLatin1String("false");
}

void UmsDevice::saveOptions()
{
    if (!isConnected()) {
        return;
    }

    QString path=spaceInfo.path();
    QFile file(path+constSettingsFile);
    QString fixedPath(audioFolder);

    if (fixedPath.startsWith(path)) {
        fixedPath=QLatin1String("./")+fixedPath.mid(path.length());
    }

    DeviceOptions def;
    if (file.open(QIODevice::WriteOnly|QIODevice::Text)) {
        QTextStream out(&file);
        if (!fixedPath.isEmpty()) {
            out << constMusicFolderKey << '=' << fixedPath << '\n';
        }
        if (opts.scheme!=def.scheme) {
            out << constMusicFilenameSchemeKey << '=' << opts.scheme << '\n';
        }
        if (opts.scheme!=def.scheme) {
            out << constVfatSafeKey << '=' << toString(opts.vfatSafe) << '\n';
        }
        if (opts.asciiOnly!=def.asciiOnly) {
            out << constAsciiOnlyKey << '=' << toString(opts.asciiOnly) << '\n';
        }
        if (opts.ignoreThe!=def.ignoreThe) {
            out << constIgnoreTheKey << '=' << toString(opts.ignoreThe) << '\n';
        }
        if (opts.replaceSpaces!=def.replaceSpaces) {
            out << constReplaceSpacesKey << '=' << toString(opts.replaceSpaces) << '\n';
        }

        foreach (const QString &u, unusedParams) {
            out << u << '\n';
        }
    }
}

void UmsDevice::saveProperties(const QString &newPath, const DeviceOptions &newOpts)
{
    QString nPath=Utils::fixPath(newPath);
    if (configured && opts==newOpts && nPath==audioFolder) {
        return;
    }

    configured=true;
    bool diffCacheSettings=opts.useCache!=newOpts.useCache;
    opts=newOpts;
    if (diffCacheSettings) {
        if (opts.useCache) {
            saveCache();
        } else {
            removeCache();
        }
    }
    QString oldPath=audioFolder;
    if (!isConnected()) {
        return;
    }

    audioFolder=nPath;
    saveOptions();

    FsDevice::writeOpts(spaceInfo.path()+constCantataSettingsFile, opts, false);

    if (oldPath!=audioFolder) {
        rescan();
    }
}
