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

#include "actiondialog.h"
#include "devicesmodel.h"
#include "device.h"
#include "utils.h"
#include "devicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "settings.h"
#include "musiclibrarymodel.h"
#include "dirviewmodel.h"
#include "albumsmodel.h"
#include "mpdparseutils.h"
#include "mpdconnection.h"
#include "encoders.h"
#include "localize.h"
#include <KDE/KGlobal>
#include <KDE/KMessageBox>
#include <KDE/KIO/FileCopyJob>
#include <KDE/KIO/Job>
#include <KDE/KDiskFreeSpaceInfo>
#include <kcapacitybar.h>

static int iCount=0;

int ActionDialog::instanceCount()
{
    return iCount;
}

enum Pages
{
    PAGE_START,
    PAGE_ERROR,
    PAGE_SKIP,
    PAGE_PROGRESS
};

ActionDialog::ActionDialog(QWidget *parent)
    : Dialog(parent)
    , currentDev(0)
{
    iCount++;
    setButtons(Ok|Cancel);
    setAttribute(Qt::WA_DeleteOnClose);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    errorIcon->setPixmap(QIcon::fromTheme("dialog-error").pixmap(64, 64));
    skipIcon->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(64, 64));
    configureSourceButton->setIcon(QIcon::fromTheme("configure"));
    configureDestButton->setIcon(QIcon::fromTheme("configure"));
    connect(configureSourceButton, SIGNAL(clicked()), SLOT(configureSource()));
    connect(configureDestButton, SIGNAL(clicked()), SLOT(configureDest()));
    connect(this, SIGNAL(update()), MPDConnection::self(), SLOT(update()));
}

ActionDialog::~ActionDialog()
{
    iCount--;
}

void ActionDialog::copy(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs)
{
    init(srcUdi, dstUdi, songs, Copy);
    Device *dev=getDevice(sourceUdi.isEmpty() ? destUdi : sourceUdi);

    if (!dev) {
        deleteLater();
        return;
    }

    // check space...
    haveVariousArtists=false;
    qint64 spaceRequired=0;
    foreach (const Song &s, songsToAction) {
        if (s.size>0) {
            spaceRequired+=s.size;
        }
        if (!haveVariousArtists && s.isVariousArtists()) {
            haveVariousArtists=true;
        }
    }

    qint64 spaceAvailable=0;
    double usedCapacity=0.0;
    QString capacityString;

    if (sourceUdi.isEmpty()) { // If source UDI is empty, then we are copying from MPD->device, so need device free space.
        spaceAvailable=dev->freeSpace();
        capacityString=dev->capacityString();
        usedCapacity=dev->usedCapacity();
    } else {
        KDiskFreeSpaceInfo inf=KDiskFreeSpaceInfo::freeSpaceInfo(Settings::self()->mpdDir());
        spaceAvailable=inf.size()-inf.used();
        usedCapacity=(inf.used()*1.0)/(inf.size()*1.0);
        capacityString=i18n("%1 free", KGlobal::locale()->formatByteSize(inf.size()-inf.used()), 1);
    }

    bool enoughSpace=spaceAvailable>spaceRequired;
    #ifdef ENABLE_REMOTE_DEVICES
    if (!enoughSpace && sourceUdi.isEmpty() && 0==spaceAvailable && usedCapacity<0.0 && (Device::RemoteFs==dev->devType() || Device::RemoteKio==dev->devType())) {
        enoughSpace=true;
    }
    #endif
    if (enoughSpace || (sourceUdi.isEmpty() && Encoders::getAvailable().count())) {
        overwrite->setChecked(Settings::self()->overwriteSongs());
        sourceLabel->setText(QLatin1String("<b>")+(sourceUdi.isEmpty() ? i18n("Local Music Library") : dev->data())+QLatin1String("</b>"));
        destinationLabel->setText(QLatin1String("<b>")+(destUdi.isEmpty() ? i18n("Local Music Library") : dev->data())+QLatin1String("</b>"));
        namingOptions.load("mpd");
        setPage(PAGE_START);
        mode=Copy;
        capacity->setText(capacityString);
        capacity->setValue((usedCapacity*100)+0.5);
        capacity->setDrawTextMode(KCapacityBar::DrawTextInline);

        bool destIsDev=sourceUdi.isEmpty();
        configureDestLabel->setVisible(destIsDev && !dev->isConfigured());
        configureSourceLabel->setVisible(!destIsDev && !dev->isConfigured());
        show();
        if (!enoughSpace) {
            KMessageBox::information(this, i18n("There is insufficient space left on the destination device.\n"
                                                "The selected songs consume %1, but there is only %2 left.\n"
                                                "The songs will need to be transcoded to a smaller filesize in order to be successfully copied.",
                                                KGlobal::locale()->formatByteSize(spaceRequired),
                                                KGlobal::locale()->formatByteSize(spaceAvailable)));
        }
    } else {
        KMessageBox::error(parentWidget(), i18n("There is insufficient space left on the destination.\n"
                                                "The selected songs consume %1, but there is only %2 left.",
                                                KGlobal::locale()->formatByteSize(spaceRequired),
                                                KGlobal::locale()->formatByteSize(spaceAvailable)));
        deleteLater();
    }
}

void ActionDialog::remove(const QString &udi, const QList<Song> &songs)
{
    init(udi, QString(), songs, Remove);
    QString baseDir;

    if (udi.isEmpty()) {
        baseDir=Settings::self()->mpdDir();
    } else {
        Device *dev=getDevice(udi);

        if (!dev) {
            deleteLater();
            return;
        }

        if (Device::RemoteKio!=dev->devType()) {
            baseDir=dev->path();
        }
    }

    setPage(PAGE_PROGRESS);
    foreach (const Song &s, songsToAction) {
        dirsToClean.insert(baseDir+MPDParseUtils::getDir(s.file));
    }
    show();
    doNext();
}

void ActionDialog::init(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs, Mode m)
{
    resize(500, 160);
    sourceUdi=srcUdi;
    destUdi=dstUdi;
    songsToAction=songs;
    mode=m;
    setCaption(Copy==mode ? i18n("Copy Songs") : i18n("Delete Songs"));
    qSort(songsToAction);
    progressLabel->setText(QString());
    progressBar->setValue(0);
    progressBar->setRange(0, Copy==mode ? (songsToAction.count()*100) : (songsToAction.count()+1));
    autoSkip=false;
    performingAction=false;
    paused=false;
    actionedSongs.clear();
    currentDev=0;
    count=0;
}

void ActionDialog::slotButtonClicked(int button)
{
    switch(stack->currentIndex()) {
    case PAGE_START:
        switch (button) {
        case Ok:
            if (haveVariousArtists &&
                ((configureDestLabel->isVisible() &&
                  KMessageBox::No==KMessageBox::warningYesNo(this, i18n("<p>You have not configured the destination device.<br/>"
                                                                       "Continue with the default settings?</p>"))) ||
                 (configureSourceLabel->isVisible() &&
                  KMessageBox::No==KMessageBox::warningYesNo(this, i18n("<p>You have not configured the source device.<br/>"
                                                                       "Continue with the default settings?</p>"))))) {
                return;
            }
            Settings::self()->saveOverwriteSongs(overwrite->isChecked());
            setPage(PAGE_PROGRESS);
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
        if (KMessageBox::Yes==KMessageBox::questionYesNo(this, i18n("Are you sure you wish to cancel?"))) {
            refreshLibrary();
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

Device * ActionDialog::getDevice(const QString &udi)
{
    Device *dev=DevicesModel::self()->device(udi);
    QString error;
    if (!dev) {
        error=i18n("Device has been removed!");
    } else if (!dev->isConnected()) {
        error=i18n("Device is not connected!");
    } else if (!dev->isIdle()) {
        error=i18n("Device is busy?");
    } else if (currentDev && dev!=currentDev) {
        error=i18n("Device has been changed?");
    }

    if (error.isEmpty()) {
        return dev;
    }

    if (isVisible()) {
        setPage(PAGE_ERROR, error);
    } else {
        KMessageBox::error(parentWidget(), error);
    }

    return 0;
}

void ActionDialog::doNext()
{
    if (songsToAction.count()) {
        currentSong=origCurrentSong=songsToAction.takeFirst();
        if(Copy==mode) {
            bool copyToDev=sourceUdi.isEmpty();
            Device *dev=getDevice(copyToDev ? destUdi : sourceUdi);

            if (dev) {
                if (!currentDev) {
                    connect(dev, SIGNAL(actionStatus(int)), this, SLOT(actionStatus(int)));
                    connect(dev, SIGNAL(progress(unsigned long)), this, SLOT(copyPercent(unsigned long)));
                    currentDev=dev;
                }
                performingAction=true;
                if (copyToDev) {
                    destFile=dev->path()+dev->options().createFilename(currentSong);
                    currentSong.file=Settings::self()->mpdDir()+currentSong.file;
                    dev->addSong(currentSong, overwrite->isChecked());
                } else {
                    Song copy=currentSong;
                    if (dev->options().fixVariousArtists && currentSong.isVariousArtists()) {
                        Device::fixVariousArtists(QString(), copy, false);
                    }
                    QString fileName=namingOptions.createFilename(copy);
                    destFile=Settings::self()->mpdDir()+fileName;
                    dev->copySongTo(currentSong, Settings::self()->mpdDir(), fileName, overwrite->isChecked());
                }
            }
        } else {
            if (sourceUdi.isEmpty()) {
                performingAction=true;
                currentSong.file=Settings::self()->mpdDir()+currentSong.file;
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
        progressLabel->setText(formatSong(currentSong));
    } else if (Remove==mode && dirsToClean.count()) {
        Device *dev=sourceUdi.isEmpty() ? 0 : DevicesModel::self()->device(sourceUdi);
        if (sourceUdi.isEmpty() || dev) {
            progressLabel->setText(i18n("Clearing unused folders"));
            if (dev) {
                dev->cleanDirs(dirsToClean);
            } else {
                foreach (const QString &d, dirsToClean) {
                    Utils::cleanDir(d, Settings::self()->mpdDir(), QString());
                }
            }
        }
        dirsToClean.clear();
        incProgress();
        doNext();
    } else {
        refreshLibrary();
        emit completed();
        accept();
    }
}

void ActionDialog::actionStatus(int status)
{
    int origStatus=status;
    if (Device::Ok!=status && Device::NotConnected!=status && autoSkip) {
        status=Device::Ok;
    }
    switch (status) {
    case Device::Ok:
        performingAction=false;
        if (Device::Ok==origStatus) {
            actionedSongs.append(currentSong);
        }
//         if (1==actionedSongs.count() && ( (Copy==mode && !sourceUdi.isEmpty()) ||
//                                           (Remove==mode && sourceUdi.isEmpty()) ) ) {
//             // Cache is now out of date, so need to remove!
//             MusicLibraryModel::self()->removeCache();
//         }
        if (!paused) {
            incProgress();
            doNext();
        }
        break;
    case Device::FileExists:
        setPage(PAGE_SKIP, i18n("The destination filename already exists!<hr/>%1", formatSong(currentSong, true)));
        break;
    case Device::SongExists:
        setPage(PAGE_SKIP, i18n("Song already exists!<hr/>%1", formatSong(currentSong)));
        break;
    case Device::SongDoesNotExist:
        setPage(PAGE_SKIP, i18n("Song does not exist!<hr/>%1", formatSong(currentSong)));
        break;
    case Device::DirCreationFaild:
        setPage(PAGE_SKIP, i18n("Failed to create destination folder!<br/>Please check you have sufficient permissions.<hr/>%1", formatSong(currentSong, true)));
        break;
    case Device::SourceFileDoesNotExist:
        setPage(PAGE_SKIP, i18n("Source file no longer exists?<br/><br/<hr/>%1", formatSong(currentSong, true)));
        break;
    case Device::Failed:
        setPage(PAGE_SKIP, Copy==mode ? i18n("Failed to copy.<hr/>%1", formatSong(currentSong))
                                      : i18n("Failed to delete.<hr/>%1", formatSong(currentSong)));
        break;
    case Device::NotConnected:
        setPage(PAGE_ERROR, i18n("Not connected to device.<hr/>%1", formatSong(currentSong)));
        break;
    case Device::CodecNotAvailable:
        setPage(PAGE_ERROR, i18n("Selected codec is not available.<hr/>%1", formatSong(currentSong)));
        break;
    case Device::TranscodeFailed:
        setPage(PAGE_SKIP, i18n("Transcoding failed.<br/><br/<hr/>%1", formatSong(currentSong)));
        break;
    case Device::FailedToCreateTempFile:
        setPage(PAGE_ERROR, i18n("Failed to create temporary file.<br/>(Required for transcoding to MTP devices.)<hr/>%1", formatSong(currentSong)));
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
        connect(dlg, SIGNAL(updatedSettings(const QString &, const QString &, const Device::Options &)),
                SLOT(saveProperties(const QString &, const QString &, const Device::Options &)));
        dlg->setCaption(i18n("Local Music Library Properties"));
        dlg->show(Settings::self()->mpdDir(), QString(), namingOptions, DevicePropertiesWidget::Prop_Basic);
    } else {
        Device *dev=DevicesModel::self()->device(udi);
        if (dev) {
            dev->configure(this);
        }
    }
}

void ActionDialog::saveProperties(const QString &path, const QString &coverFile, const Device::Options &opts)
{
    Q_UNUSED(path)
    Q_UNUSED(coverFile)
    namingOptions=opts;
    namingOptions.save("mpd");
}

void ActionDialog::setPage(int page, const QString &msg)
{
    stack->setCurrentIndex(page);

    switch(page)
    {
        case PAGE_START:
            setButtons(Ok|Cancel);
            break;
        case PAGE_PROGRESS:
            actionLabel->startAnimation();
            setButtons(Cancel);
            break;
        case PAGE_SKIP:
            actionLabel->stopAnimation();
            skipText->setText(i18n("<b>Error</b><br/>")+msg);
            skipText->setToolTip(formatSong(currentSong, true));
            if (songsToAction.count()) {
                setButtons(Cancel|User1|User2|User3);
                setButtonText(User1, i18n("Skip"));
                setButtonText(User2, i18n("Auto Skip"));
            } else {
                setButtons(Cancel|User3);
            }
            setButtonText(User3, i18n("Retry"));
            break;
        case PAGE_ERROR:
            actionLabel->stopAnimation();
            stack->setCurrentIndex(PAGE_ERROR);
            errorText->setText(i18n("<b>Error</b><br/>")+msg);
            errorText->setToolTip(formatSong(currentSong, true));
            setButtons(Cancel);
            break;
    }
}

QString ActionDialog::formatSong(const Song &s, bool showFiles)
{
    return showFiles
            ? Copy==mode
                ? i18n("<table>"
                       "<tr><td align=\"right\">Artist:</td><td>%1</td></tr>"
                       "<tr><td align=\"right\">Album:</td><td>%2</td></tr>"
                       "<tr><td align=\"right\">Track:</td><td>%3</td></tr>"
                       "<tr><td align=\"right\">Source file:</td><td>%4</td></tr>"
                       "<tr><td align=\"right\">Destination file:</td><td>%5</td></tr>"
                       "</table>", s.albumArtist(), s.album,
                       s.trackAndTitleStr(Song::isVariousArtists(s.albumArtist()) && !Song::isVariousArtists(s.artist)), s.file, destFile)
                : i18n("<table>"
                       "<tr><td align=\"right\">Artist:</td><td>%1</td></tr>"
                       "<tr><td align=\"right\">Album:</td><td>%2</td></tr>"
                       "<tr><td align=\"right\">Track:</td><td>%3</td></tr>"
                       "<tr><td align=\"right\">File:</td><td>%4</td></tr>"
                       "</table>", s.albumArtist(), s.album,
                       s.trackAndTitleStr(Song::isVariousArtists(s.albumArtist()) && !Song::isVariousArtists(s.artist)), s.file)
            : i18n("<table>"
                   "<tr><td align=\"right\">Artist:</td><td>%1</td></tr>"
                   "<tr><td align=\"right\">Album:</td><td>%2</td></tr>"
                   "<tr><td align=\"right\">Track:</td><td>%3</td></tr>"
                   "</table>", s.albumArtist(), s.album,
                   s.trackAndTitleStr(Song::isVariousArtists(s.albumArtist()) && !Song::isVariousArtists(s.artist)));
}

void ActionDialog::refreshLibrary()
{
    actionLabel->stopAnimation();
    if (!actionedSongs.isEmpty()) {
        if ( (Copy==mode && !sourceUdi.isEmpty()) ||
             (Remove==mode && sourceUdi.isEmpty()) ) {
            AlbumsModel::self()->update(MusicLibraryModel::self()->root());
            emit update();
        } else if ( (Copy==mode && sourceUdi.isEmpty()) ||
                    (Remove==mode && !sourceUdi.isEmpty()) ) {
            Device *dev=DevicesModel::self()->device(sourceUdi.isEmpty() ? destUdi : sourceUdi);

            if (dev) {
                dev->saveCache();
            }
        }
    }
}

void ActionDialog::removeSong(const Song &s)
{
    if (!QFile::exists(s.file)) {
        actionStatus(Device::SourceFileDoesNotExist);
        return;
    }

    KIO::SimpleJob *job=KIO::file_delete(KUrl(s.file), KIO::HideProgressInfo);
    connect(job, SIGNAL(result(KJob *)), SLOT(removeSongResult(KJob *)));
}

void ActionDialog::removeSongResult(KJob *job)
{
    if (job->error()) {
        actionStatus(Device::Failed);
    } else {
        MusicLibraryModel::self()->removeSongFromList(currentSong);
        DirViewModel::self()->removeFileFromList(currentSong.file);
        actionStatus(Device::Ok);
    }
}

void ActionDialog::copyPercent(unsigned long percent)
{
    progressBar->setValue((100*count)+percent);
}

void ActionDialog::incProgress()
{
    if (Copy==mode) {
        count++;
        progressBar->setValue(100*count);
    } else {
        progressBar->setValue(progressBar->value()+1);
    }
}

