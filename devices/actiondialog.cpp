/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "actiondialog.h"
#include "models/devicesmodel.h"
#include "device.h"
#include "support/utils.h"
#include "devicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "gui/settings.h"
#include "models/musiclibraryproxymodel.h"
#include "mpd-interface/mpdparseutils.h"
#include "mpd-interface/mpdconnection.h"
#include "encoders.h"
#include "support/messagebox.h"
#include "filejob.h"
#include "freespaceinfo.h"
#include "widgets/icons.h"
#include "config.h"
#include "tags/tags.h"
#include "widgets/treeview.h"
#ifdef ENABLE_ONLINE_SERVICES____TODO
#include "models/onlineservicesmodel.h"
#endif
#ifdef ENABLE_REPLAYGAIN_SUPPORT
#include "replaygain/rgdialog.h"
#endif
#include <QFile>
#include <QTimer>
#include <QStyle>
#ifdef QT_QTDBUS_FOUND
#include <QDBusConnection>
#endif
#include <algorithm>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0

static int iCount=0;

int ActionDialog::instanceCount()
{
    return iCount;
}

enum Pages
{
    PAGE_SIZE_CALC,
    PAGE_INSUFFICIENT_SIZE,
    PAGE_START,
    PAGE_ERROR,
    PAGE_SKIP,
    PAGE_PROGRESS
};

ActionDialog::ActionDialog(QWidget *parent)
    : Dialog(parent)
    , spaceRequired(0)
    , mpdConfigured(false)
    , currentDev(nullptr)
    , songDialog(nullptr)
{
    iCount++;
    setButtons(Ok|Cancel);
    setDefaultButton(Cancel);
    setAttribute(Qt::WA_DeleteOnClose);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    errorIcon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxCritical).pixmap(64, 64));
    skipIcon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(64, 64));
    configureSourceButton->setIcon(Icons::self()->configureIcon);
    configureDestButton->setIcon(Icons::self()->configureIcon);
    connect(configureSourceButton, SIGNAL(clicked()), SLOT(configureSource()));
    connect(configureDestButton, SIGNAL(clicked()), SLOT(configureDest()));
    connect(this, SIGNAL(update()), MPDConnection::self(), SLOT(update()));
//    connect(songCount, SIGNAL(leftClickedUrl()), SLOT(showSongs()));
    #ifdef QT_QTDBUS_FOUND
    unityMessage=QDBusMessage::createSignal("/Cantata", "com.canonical.Unity.LauncherEntry", "Update");
    #endif
}

ActionDialog::~ActionDialog()
{
    iCount--;
    updateUnity(true);
}

void ActionDialog::controlInfoLabel(Device *dev)
{
    DeviceOptions opts;
    if (sourceIsAudioCd) {
        opts.load(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);
    } else if (sourceUdi.isEmpty()) {
        opts=dev->options();
    }

    if (opts.transcoderCodec.isEmpty()) {
        codecLabel->setVisible(false);
        codec->setVisible(false);
    } else {
        codecLabel->setVisible(true);
        codec->setVisible(true);
        Encoders::Encoder encoder=Encoders::getEncoder(opts.transcoderCodec);
        if (Encoders::getEncoder(opts.transcoderCodec).codec.isEmpty()) {
            codec->setText(tr("<b>INVALID</b>"));
        } else if (encoder.values.count()>1) {
            int settingIndex=0;
            bool increase=encoder.values.at(0).value<encoder.values.at(1).value;
            int index=0;
            for (const Encoders::Setting &s: encoder.values) {
                if ((increase && s.value>opts.transcoderValue) || (!increase && s.value<opts.transcoderValue)) {
                    break;
                } else {
                    settingIndex=index;
                }
                index++;
            }
            codec->setText(QString("%1 (%2)").arg(encoder.name).arg(encoder.values.at(settingIndex).descr)+
                           (!sourceIsAudioCd && DeviceOptions::TW_IfDifferent==opts.transcoderWhen ? tr("<i>(When different)</i>") : QString()));
        } else {
            codec->setText(encoder.name+
                           (!sourceIsAudioCd && DeviceOptions::TW_IfDifferent==opts.transcoderWhen ? tr("<i>(When different)</i>") : QString()));
        }
    }
}

void ActionDialog::deviceRenamed()
{
    Device *dev=getDevice(sourceUdi.isEmpty() ? destUdi : sourceUdi);
    if (!dev) {
        return;
    }

    if (Remove==mode || (Copy==mode && !sourceUdi.isEmpty())) {
        sourceLabel->setText(QLatin1String("<b>")+dev->data()+QLatin1String("</b>"));
    } else {
        destinationLabel->setText(QLatin1String("<b>")+dev->data()+QLatin1String("</b>"));
    }
}

void ActionDialog::updateSongCountLabel()
{
    QSet<QString> artists;
    QSet<QString> albums;

    for (const Song &s: songsToAction) {
        artists.insert(s.albumArtist());
        albums.insert(s.albumArtist()+"--"+s.album);
    }

    songCount->setText(tr("Artists:%1, Albums:%2, Songs:%3").arg(artists.count()).arg(albums.count()).arg(songsToAction.count()));
}

void ActionDialog::controlInfoLabel()
{
    controlInfoLabel(getDevice(sourceUdi.isEmpty() ? destUdi : sourceUdi));
}

void ActionDialog::calcFileSize()
{
    Device *dev=nullptr;
    int toCalc=qMin(50, songsToCalcSize.size());
    for (int i=0; i<toCalc; ++i) {
        Song s=songsToCalcSize.takeAt(0);
        quint32 size=s.size;
        if (sourceIsAudioCd) {
            size/=18; // Just guess at a compression ratio... ~18x ~=80kbps MP3
        } else if (0==size) {
            if (sourceUdi.isEmpty()) {
                size=QFileInfo(MPDConnection::self()->getDetails().dir+s.file).size();
            } else {
                if (!dev) {
                    dev=getDevice(sourceUdi.isEmpty() ? destUdi : sourceUdi);
                }

                if (!dev) {
                    deleteLater();
                    return;
                }

                if (QFile::exists(dev->path()+s.file)) { // FS device...
                    size=QFileInfo(dev->path()+s.file).size();
                }
            }
        }
        if (size>0) {
            spaceRequired+=size;
        }
        if (!haveVariousArtists && s.isVariousArtists()) {
            haveVariousArtists=true;
        }
    }
    fileSizeProgress->setValue(fileSizeProgress->value()+toCalc);
    if (!songsToCalcSize.isEmpty()) {
        QTimer::singleShot(0, this, SLOT(calcFileSize()));
    } else {
        qint64 spaceAvailable=0;
        double usedCapacity=0.0;
        QString capacityString;
        //bool isFromOnline=sourceUdi.startsWith(OnlineServicesModel::constUdiPrefix);

        if (!dev) {
            dev=getDevice(sourceUdi.isEmpty() ? destUdi : sourceUdi);
        }

        if (!dev) {
            deleteLater();
            return;
        }

        if (sourceUdi.isEmpty()) { // If source UDI is empty, then we are copying from MPD->device, so need device free space.
            spaceAvailable=dev->freeSpace();
            capacityString=dev->capacityString();
            usedCapacity=dev->usedCapacity();
        } else {
            FreeSpaceInfo inf=FreeSpaceInfo(MPDConnection::self()->getDetails().dir);
            spaceAvailable=inf.size()-inf.used();
            usedCapacity=(inf.used()*1.0)/(inf.size()*1.0);
            capacityString=tr("%1 free").arg(Utils::formatByteSize(inf.size()-inf.used()));
        }

        bool enoughSpace=spaceAvailable>spaceRequired;
        #ifdef ENABLE_REMOTE_DEVICES
        if (!enoughSpace && sourceUdi.isEmpty() && 0==spaceAvailable && usedCapacity<0.0 && Device::RemoteFs==dev->devType()) {
            enoughSpace=true;
        }
        #endif
        if (enoughSpace || (sourceUdi.isEmpty() && Encoders::getAvailable().count())) {
            QString mpdCfgName=MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name);
            overwrite->setChecked(Settings::self()->overwriteSongs());
            sourceLabel->setText(QLatin1String("<b>")+(sourceUdi.isEmpty()
                                    ? tr("Local Music Library")
                                    : sourceIsAudioCd
                                        ? tr("Audio CD")
                                        : dev->data())+QLatin1String("</b>"));
            destinationLabel->setText(QLatin1String("<b>")+(destUdi.isEmpty() ? tr("Local Music Library") : dev->data())+QLatin1String("</b>"));
            namingOptions.load(mpdCfgName, true);

            capacity->update(capacityString, (usedCapacity*100)+0.5);

            bool destIsDev=sourceUdi.isEmpty();
            mpdConfigured=DeviceOptions::isConfigured(mpdCfgName, true);
            configureDestLabel->setVisible((destIsDev && !dev->isConfigured()) || (!destIsDev && !mpdConfigured));
            //configureSourceButton->setVisible(!isFromOnline);
            //configureSourceLabel->setVisible(!isFromOnline && ((!destIsDev && !dev->isConfigured()) || (destIsDev && !mpdConfigured)));
            configureSourceButton->setVisible(false);
            configureSourceLabel->setVisible(false);
            songCount->setVisible(!sourceIsAudioCd);
            songCountLabel->setVisible(!sourceIsAudioCd);
            if (!sourceIsAudioCd) {
                updateSongCountLabel();
            }
            if (enoughSpace) {
                setPage(PAGE_START);
            } else {
                setPage(PAGE_INSUFFICIENT_SIZE);
                sizeInfoIcon->setPixmap(*(skipIcon->pixmap()));
                sizeInfoText->setText(tr("There is insufficient space left on the destination device.\n\n"
                                          "The selected songs consume %1, but there is only %2 left.\n"
                                          "The songs will need to be transcoded to a smaller filesize in order to be successfully copied.")
                                          .arg(Utils::formatByteSize(spaceRequired))
                                          .arg(Utils::formatByteSize(spaceAvailable)));
            }
        } else {
            setPage(PAGE_INSUFFICIENT_SIZE);
            setButtons(Cancel);
            sizeInfoIcon->setPixmap(*(errorIcon->pixmap()));
            sizeInfoText->setText(tr("There is insufficient space left on the destination.\n\n"
                                      "The selected songs consume %1, but there is only %2 left.")
                                      .arg(Utils::formatByteSize(spaceRequired))
                                      .arg(Utils::formatByteSize(spaceAvailable)));
        }
    }
}

void ActionDialog::sync(const QString &devId, const QList<Song> &libSongs, const QList<Song> &devSongs)
{
    // If only copying one way, then just use standard copying...
    bool toLib=libSongs.isEmpty();
    if (toLib || devSongs.isEmpty()) {
        copy(toLib ? devId : QString(), toLib ? QString() : devId, toLib ? devSongs : libSongs);
        setCaption(toLib ? tr("Copy Songs To Library") : tr("Copy Songs To Device"));
        return;
    }

    init(toLib ? devId : QString(), toLib ? QString() : devId, toLib ? devSongs : libSongs, Sync);
    Device *dev=getDevice(devId);

    if (!dev) {
        deleteLater();
        return;
    }

    progressBar->setRange(0, (libSongs.count()+devSongs.count())*100);

    sourceIsAudioCd=false;
    controlInfoLabel(dev);
    fileSizeProgress->setMinimum(0);
    fileSizeProgress->setMaximum(songsToAction.size());
    songsToCalcSize=songsToAction;
    syncSongs=devSongs;
    setCaption(toLib ? tr("Copy Songs To Library") : tr("Copy Songs To Device"));
    setPage(PAGE_SIZE_CALC);
    show();
    calcFileSize();
}

void ActionDialog::copy(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs)
{
    init(srcUdi, dstUdi, songs, Copy);
    Device *dev=getDevice(sourceUdi.isEmpty() ? destUdi : sourceUdi);

    if (!dev) {
        deleteLater();
        return;
    }

    sourceIsAudioCd=Device::AudioCd==dev->devType();
    controlInfoLabel(dev);
    fileSizeProgress->setMinimum(0);
    fileSizeProgress->setMaximum(songs.size());
    songsToCalcSize=songs;
    setPage(PAGE_SIZE_CALC);
    show();
    calcFileSize();
}

void ActionDialog::remove(const QString &udi, const QList<Song> &songs)
{
    init(udi, QString(), songs, Remove);
    codecLabel->setVisible(false);
    codec->setVisible(false);
    songCountLabel->setVisible(false);
    songCount->setVisible(false);
    QString baseDir;

    if (udi.isEmpty()) {
        baseDir=MPDConnection::self()->getDetails().dir;
    } else {
        Device *dev=getDevice(udi);

        if (!dev) {
            deleteLater();
            return;
        }

        baseDir=dev->path();
    }

    setPage(PAGE_PROGRESS);
    for (const Song &s: songsToAction) {
        dirsToClean.insert(baseDir+Utils::getDir(s.file));
    }
    show();
    doNext();
}

void ActionDialog::init(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs, Mode m)
{
    resize(500, 160);
    sourceUdi=srcUdi;
    destUdi=dstUdi;
    sourceIsAudioCd=false;
    songsToAction=songs;
    mode=m;
    setCaption(Copy==mode || Sync==mode ? tr("Copy Songs") : tr("Delete Songs"));
    std::sort(songsToAction.begin(), songsToAction.end());
    progressLabel->setText(QString());
    progressBar->setValue(0);
    progressBar->setRange(0, (Copy==mode || Sync==mode ? songsToAction.count() : (songsToAction.count()+1))*100);
    autoSkip=false;
    performingAction=false;
    paused=false;
    actionedSongs.clear();
    skippedSongs.clear();
    currentPercent=0;
    currentDev=nullptr;
    count=0;
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    albumsWithoutRgTags.clear();
    #endif
    updateUnity(false);
}

void ActionDialog::slotButtonClicked(int button)
{
    switch(stack->currentIndex()) {
    case PAGE_SIZE_CALC:
        Dialog::slotButtonClicked(button);
        break;
    case PAGE_INSUFFICIENT_SIZE:
        if (Ok==button) {
            setPage(PAGE_START);
        } else {
            Dialog::slotButtonClicked(button);
        }
        break;
    case PAGE_START:
        switch (button) {
        case Ok:
            if (haveVariousArtists &&
                ((configureDestLabel->isVisible() && sourceUdi.isEmpty() && // Only warn if copying FROM library
                  MessageBox::No==MessageBox::warningYesNo(this, tr("You have not configured the destination device.\n\n"
                                                                      "Continue with the default settings?"), tr("Not Configured"),
                                                           GuiItem(tr("Use Defaults")), StdGuiItem::cancel())) ||
                 (configureSourceLabel->isVisible() && !sourceUdi.isEmpty() && // Only warn if copying TO library
                  MessageBox::No==MessageBox::warningYesNo(this, tr("You have not configured the source device.\n\n"
                                                                      "Continue with the default settings?"), tr("Not Configured"),
                                                           GuiItem(tr("Use Defaults")), StdGuiItem::cancel())) ) ) {
                return;
            }
            Settings::self()->saveOverwriteSongs(overwrite->isChecked());
            setPage(PAGE_PROGRESS);
//            hideSongs();
            doNext();
            break;
        case Cancel:
            refreshLibrary();
            reject();
            // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
            Dialog::slotButtonClicked(button);
            break;
        default:
            Dialog::slotButtonClicked(button);
            break;
        }
        break;
    case PAGE_SKIP:
        setPage(PAGE_PROGRESS);
        switch(button) {
        case User1:
            skippedSongs.append(currentSong);
            incProgress();
            doNext();
            break;
        case User2:
            autoSkip=true;
            incProgress();
            doNext();
            break;
        case User3:
            songsToAction.prepend(origCurrentSong);
            doNext();
            break;
        default:
            refreshLibrary();
            reject();
            // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
            Dialog::slotButtonClicked(button);
            break;
        }
        break;
    case PAGE_ERROR:
        refreshLibrary();
        reject();
        break;
    case PAGE_PROGRESS:
        paused=true;
        if (MessageBox::Yes==MessageBox::questionYesNo(this, tr("Are you sure you wish to stop?"), tr("Stop"),
                                                       StdGuiItem::stop(), StdGuiItem::cont())) {
            Device *dev=nullptr;
            if(Copy==mode || Sync==mode) {
                dev=getDevice(sourceUdi.isEmpty() ? destUdi : sourceUdi, false);
            } else if (!sourceUdi.isEmpty()) { // Must be a delete...
                dev=getDevice(sourceUdi, false);
            }

            if (dev) {
                if (Close==button) { // Close is only enabled when saving cache...
                    disconnect(dev, SIGNAL(cacheSaved()), this, SLOT(cacheSaved()));
                } else {
                    dev->abortJob();
                }
            }
            if (Close!=button) {
                refreshLibrary();
            }
            reject();
            // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
            Dialog::slotButtonClicked(button);
        } else if (!performingAction && PAGE_PROGRESS==stack->currentIndex()) {
            paused=false;
            incProgress();
            doNext();
        }
    }
}

Device * ActionDialog::getDevice(const QString &udi, bool logErrors)
{
    #ifdef ENABLE_ONLINE_SERVICES____TODO
    if (udi.startsWith(OnlineServicesModel::constUdiPrefix)) {
        return OnlineServicesModel::self()->device(udi);
    }
    #endif
    Device *dev=DevicesModel::self()->device(udi);

    if (!logErrors) {
        return dev;
    }

    QString error;
    if (!dev) {
        error=tr("Device has been removed!");
    } else if (!dev->isConnected()) {
        error=tr("Device is not connected!");
    } else if (!dev->isIdle()) {
        error=tr("Device is busy?");
    } else if (currentDev && dev!=currentDev) {
        error=tr("Device has been changed?");
    }

    if (error.isEmpty()) {
        return dev;
    }

    if (isVisible()) {
        setPage(PAGE_ERROR, StringPairList(), error);
    } else {
        MessageBox::error(parentWidget(), error);
    }

    return nullptr;
}

void ActionDialog::doNext()
{
    currentPercent=0;
    if (songsToAction.isEmpty() && Sync==mode && !syncSongs.isEmpty()) {
        songsToAction=syncSongs;
        syncSongs.clear();
        sourceUdi=destUdi;
        destUdi=QString();
        setCaption(tr("Copy Songs To Library"));
    }

    if (songsToAction.count()) {
        currentSong=origCurrentSong=songsToAction.takeFirst();
        if(Copy==mode || Sync==mode) {
            bool copyToDev=sourceUdi.isEmpty();
            Device *dev=getDevice(copyToDev ? destUdi : sourceUdi);

            if (dev) {
                if (!currentDev) {
                    connect(dev, SIGNAL(actionStatus(int, bool)), this, SLOT(actionStatus(int, bool)));
                    connect(dev, SIGNAL(progress(int)), this, SLOT(jobPercent(int)));
                    currentDev=dev;
                }
                performingAction=true;
                if (copyToDev) {
                    destFile=dev->path()+dev->options().createFilename(currentSong);
                    currentSong.file=currentSong.filePath(MPDConnection::self()->getDetails().dir);
                    dev->addSong(currentSong, overwrite->isChecked(), !copiedCovers.contains(Utils::getDir(destFile)));
                } else {
                    Song copy=currentSong;
                    if (dev->options().fixVariousArtists && currentSong.isVariousArtists()) {
                        Device::fixVariousArtists(QString(), copy, false);
                    }
                    QString fileName=namingOptions.createFilename(copy);
                    destFile=MPDConnection::self()->getDetails().dir+fileName;
                    dev->copySongTo(currentSong, fileName, overwrite->isChecked(), !copiedCovers.contains(Utils::getDir(destFile)));
                }
            }
        } else {
            if (sourceUdi.isEmpty()) {
                performingAction=true;
                currentSong.file=MPDConnection::self()->getDetails().dir+currentSong.file;
                removeSong(currentSong);
            } else {
                Device *dev=getDevice(sourceUdi);
                if (dev) {
                    if (dev!=currentDev) {
                        connect(dev, SIGNAL(actionStatus(int)), this, SLOT(actionStatus(int)));
                        currentDev=dev;
                    }
                    performingAction=true;
                    dev->removeSong(currentSong);
                }
            }
        }
        progressLabel->setText(formatSong(currentSong, false));
    } else if (Remove==mode && dirsToClean.count()) {
        Device *dev=sourceUdi.isEmpty() ? nullptr : DevicesModel::self()->device(sourceUdi);
        if (sourceUdi.isEmpty() || dev) {
            progressLabel->setText(tr("Clearing unused folders"));
            if (dev) {
                dev->cleanDirs(dirsToClean);
            } else {
                cleanDirs();
            }
        }
        dirsToClean.clear();
    } else {
        if (!refreshLibrary()) {
            emit completed();
            accept();
            #ifdef ENABLE_REPLAYGAIN_SUPPORT
            if (Copy==mode && !albumsWithoutRgTags.isEmpty() && sourceIsAudioCd) {
                QWidget *pw=parentWidget();
                if (MessageBox::Yes==MessageBox::questionYesNo(pw, tr("Calculate ReplayGain for ripped tracks?"), tr("ReplayGain"),
                                                               GuiItem(tr("Calculate")), StdGuiItem::no())) {
                    RgDialog *dlg=new RgDialog(pw);
                    QList<Song> songs;
                    DeviceOptions opts;
                    opts.load(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);
                    Encoders::Encoder encoder=Encoders::getEncoder(opts.transcoderCodec);

                    for (const Song &s: actionedSongs) {
                        if (albumsWithoutRgTags.contains(s.album)) {
                            Song song=s;
                            song.file=encoder.changeExtension(namingOptions.createFilename(s));
                            songs.append(song);
                        }
                    }
                    dlg->show(songs, QString(), true);
                }
            }
            #endif
        }
    }
}

void ActionDialog::actionStatus(int status, bool copiedCover)
{
    int origStatus=status;
    bool wasSkip=false;
    if (Device::Ok!=status && Device::NotConnected!=status && autoSkip) {
        skippedSongs.append(currentSong);
        wasSkip=true;
        status=Device::Ok;
    }
    switch (status) {
    case Device::Ok:
        performingAction=false;
        if (Device::Ok==origStatus) {
            if (!wasSkip) {
                actionedSongs.append(currentSong);
                #ifdef ENABLE_REPLAYGAIN_SUPPORT
                if (Copy==mode && sourceIsAudioCd && !albumsWithoutRgTags.contains(currentSong.album) && Tags::readReplaygain(destFile).isEmpty()) {
                    albumsWithoutRgTags.insert(currentSong.album);
                }
                #endif
            }
            if (copiedCover) {
                copiedCovers.insert(Utils::getDir(destFile));
            }
        }
        if (!paused) {
            incProgress();
            doNext();
        }
        break;
    case Device::FileExists:
        setPage(PAGE_SKIP, formatSong(currentSong, true), tr("The destination filename already exists!"));
        break;
    case Device::SongExists:
        setPage(PAGE_SKIP, formatSong(currentSong), tr("Song already exists!"));
        break;
    case Device::SongDoesNotExist:
        setPage(PAGE_SKIP, formatSong(currentSong), tr("Song does not exist!"));
        break;
    case Device::DirCreationFaild:
        setPage(PAGE_SKIP, formatSong(currentSong, true), tr("Failed to create destination folder!<br/>Please check you have sufficient permissions."));
        break;
    case Device::SourceFileDoesNotExist:
        setPage(PAGE_SKIP, formatSong(currentSong, true), tr("Source file no longer exists?"));
        break;
    case Device::Failed:
        setPage(PAGE_SKIP, formatSong(currentSong), Copy==mode || Sync==mode ? tr("Failed to copy.") : tr("Failed to delete."));
        break;
    case Device::NotConnected:
        setPage(PAGE_ERROR, formatSong(currentSong), tr("Not connected to device."));
        break;
    case Device::CodecNotAvailable:
        setPage(PAGE_ERROR, formatSong(currentSong), tr("Selected codec is not available."));
        break;
    case Device::TranscodeFailed:
        setPage(PAGE_SKIP, formatSong(currentSong), tr("Transcoding failed."));
        break;
    case Device::FailedToCreateTempFile:
        setPage(PAGE_ERROR, formatSong(currentSong), tr("Failed to create temporary file.<br/>(Required for transcoding to MTP devices.)"));
        break;
    case Device::ReadFailed:
        setPage(PAGE_SKIP, formatSong(currentSong), tr("Failed to read source file."));
        break;
    case Device::WriteFailed:
        setPage(PAGE_SKIP, formatSong(currentSong), tr("Failed to write to destination file."));
        break;
    case Device::NoSpace:
        setPage(PAGE_SKIP, formatSong(currentSong), tr("No space left on device."));
        break;
    case Device::FailedToUpdateTags:
        setPage(PAGE_SKIP, formatSong(currentSong), tr("Failed to update metadata."));
        break;
    case Device::DownloadFailed:
        setPage(PAGE_SKIP, formatSong(currentSong), tr("Failed to download track."));
        break;
    case Device::FailedToLockDevice:
        setPage(PAGE_ERROR, formatSong(currentSong), tr("Failed to lock device."));
        break;
    case Device::Cancelled:
        break;
    default:
        break;
    }
}

void ActionDialog::configureDest()
{
    configureDestLabel->setVisible(false);
    configure(destUdi);
}

void ActionDialog::configureSource()
{
    configureSourceLabel->setVisible(false);
    configure(sourceUdi);
}

void ActionDialog::configure(const QString &udi)
{
    if (udi.isEmpty()) {
        DevicePropertiesDialog *dlg=new DevicePropertiesDialog(this);
        connect(dlg, SIGNAL(updatedSettings(const QString &, const DeviceOptions &)), SLOT(saveProperties(const QString &, const DeviceOptions &)));
        if (!mpdConfigured) {
            connect(dlg, SIGNAL(cancelled()), SLOT(saveProperties()));
        }
        dlg->setCaption(tr("Local Music Library Properties"));
        dlg->show(MPDConnection::self()->getDetails().dir, namingOptions, DevicePropertiesWidget::Prop_Basic|DevicePropertiesWidget::Prop_FileName|(sourceIsAudioCd ? DevicePropertiesWidget::Prop_Encoder : 0));
        connect(dlg, SIGNAL(destroyed()), SLOT(controlInfoLabel()));
    } else {
        Device *dev=DevicesModel::self()->device(udi);
        if (dev) {
            dev->configure(this);
            connect(dev, SIGNAL(configurationChanged()), SLOT(controlInfoLabel()));
            connect(dev, SIGNAL(renamed()), SLOT(deviceRenamed()));
        }
    }
}

void ActionDialog::saveProperties(const QString &path, const DeviceOptions &opts)
{
    Q_UNUSED(path)
    namingOptions=opts;
    namingOptions.save(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true, sourceIsAudioCd);
    mpdConfigured=true;
}

void ActionDialog::saveProperties()
{
    namingOptions.save(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true, sourceIsAudioCd);
    mpdConfigured=true;
}

void ActionDialog::setPage(int page, const StringPairList &msg, const QString &header)
{
    stack->setCurrentIndex(page);
    switch(page) {
    case PAGE_SIZE_CALC:
        fileSizeActionLabel->startAnimation();
        setButtons(Cancel);
        break;
    case PAGE_INSUFFICIENT_SIZE:
        fileSizeActionLabel->stopAnimation();
        setButtons(Ok|Cancel);
        break;
    case PAGE_START:
        fileSizeActionLabel->stopAnimation();
        setButtons(Ok|Cancel);
        if (Sync==mode) {
            slotButtonClicked(Ok);
        }
        break;
    case PAGE_PROGRESS:
        actionLabel->startAnimation();
        setButtons(Cancel);
        break;
    case PAGE_SKIP:
        actionLabel->stopAnimation();
        skipText->setText(msg, QLatin1String("<b>")+tr("Error")+QLatin1String("</b><br/>")+header+
                          (header.isEmpty() ? QString() : QLatin1String("<br/><br/>")));
        if (songsToAction.count()) {
            setButtons(Cancel|User1|User2|User3);
            setButtonText(User1, tr("Skip"));
            setButtonText(User2, tr("Auto Skip"));
        } else {
            setButtons(Cancel|User3);
        }
        setButtonText(User3, tr("Retry"));
        break;
    case PAGE_ERROR:
        actionLabel->stopAnimation();
        stack->setCurrentIndex(PAGE_ERROR);
        errorText->setText(msg, QLatin1String("<b>")+tr("Error")+QLatin1String("</b><br/>")+header+
                           (header.isEmpty() ? QString() : QLatin1String("<br/><br/>")));
        setButtons(Cancel);
        break;
    }
}

ActionDialog::StringPairList ActionDialog::formatSong(const Song &s, bool showFiles)
{
    StringPairList str;
    str.append(StringPair(tr("Artist:"), s.albumArtist()));
    str.append(StringPair(tr("Album:"), s.album));
    str.append(StringPair(tr("Track:"), s.trackAndTitleStr()));

    if (showFiles) {
        if (Copy==mode || Sync==mode) {
            str.append(StringPair(tr("Source file:"), DevicesModel::fixDevicePath(s.filePath())));
            str.append(StringPair(tr("Destination file:"), DevicesModel::fixDevicePath(destFile)));
        } else {
            str.append(StringPair(tr("File:"), DevicesModel::fixDevicePath(s.filePath())));
        }
    }
    
    return str;
}

bool ActionDialog::refreshLibrary()
{
    actionLabel->stopAnimation();
    if (!actionedSongs.isEmpty()) {
        if (Sync==mode) {
            emit update();
            Device *dev=DevicesModel::self()->device(sourceUdi.isEmpty() ? destUdi : sourceUdi);

            if (dev && dev->options().useCache) {
                connect(dev, SIGNAL(cacheSaved()), this, SLOT(cacheSaved()));
                dev->saveCache();
                progressLabel->setText(tr("Saving cache"));
                setButtons(Close);
                return true;
            }
        } else if ( (Copy==mode && !sourceUdi.isEmpty()) ||
                    (Remove==mode && sourceUdi.isEmpty()) ) {
//            MusicLibraryModel::self()->checkForNewSongs();
//            AlbumsModel::self()->update(MusicLibraryModel::self()->root());
            emit update();
        } else if ( (Copy==mode && sourceUdi.isEmpty()) ||
                    (Remove==mode && !sourceUdi.isEmpty()) ) {
            Device *dev=DevicesModel::self()->device(sourceUdi.isEmpty() ? destUdi : sourceUdi);

            if (dev && dev->options().useCache) {
                connect(dev, SIGNAL(cacheSaved()), this, SLOT(cacheSaved()));
                dev->saveCache();
                progressLabel->setText(tr("Saving cache"));
                setButtons(Close);
                return true;
            }
        }
    }
    return false;
}

void ActionDialog::removeSong(const Song &s)
{
    if (!QFile::exists(s.file)) {
        actionStatus(Device::SourceFileDoesNotExist);
        return;
    }

    DeleteJob *job=new DeleteJob(s.file, true);
    connect(job, SIGNAL(result(int)), SLOT(removeSongResult(int)));
    job->start();
}

void ActionDialog::removeSongResult(int status)
{
    FileJob::finished(sender());
    if (Device::Ok!=status) {
        actionStatus(status);
    } else {
//        MusicLibraryModel::self()->removeSongFromList(currentSong);
//        DirViewModel::self()->removeFileFromList(currentSong.file);
        actionStatus(Device::Ok);
    }
}

void ActionDialog::cleanDirs()
{
    CleanJob *job=new CleanJob(dirsToClean, MPDConnection::self()->getDetails().dir, QString());
    connect(job, SIGNAL(result(int)), SLOT(cleanDirsResult(int)));
    connect(job, SIGNAL(percent(int)), SLOT(jobPercent(int)));
    job->start();
}

void ActionDialog::cleanDirsResult(int status)
{
    FileJob::finished(sender());
    actionStatus(status);
}

void ActionDialog::jobPercent(int percent)
{
    if (percent!=currentPercent) {
        progressBar->setValue((100*count)+percent);
        updateUnity(false);
        currentPercent=percent;
        if (PAGE_PROGRESS==stack->currentIndex()) {
            progressLabel->setText(formatSong(currentSong, false));
        }
    }
}

void ActionDialog::incProgress()
{
    count++;
    progressBar->setValue(100*count);
    updateUnity(false);
}

void ActionDialog::updateUnity(bool finished)
{
    #ifdef QT_QTDBUS_FOUND
    QList<QVariant> args;
    double progress = finished || progressBar->maximum()<1 ? 0.0 : (progressBar->value()/(progressBar->maximum()*1.0));
    bool showProgress = progress>-0.1 && progress < 100.0 && !finished;
    QMap<QString, QVariant> props;
    props["count-visible"]=!finished && songsToAction.count()>0;
    props["count"]=(long long)songsToAction.count();
    props["progress-visible"]=showProgress;
    props["progress"]=showProgress ? progress : 0.0;
    args.append("application://cantata.desktop");
    args.append(props);
    unityMessage.setArguments(args);
    QDBusConnection::sessionBus().send(unityMessage);
    #else
    Q_UNUSED(finished)
    #endif
}

void ActionDialog::cacheSaved()
{
    emit completed();
    accept();
}

#include "moc_actiondialog.cpp"
