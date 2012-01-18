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
#include "devicepropertiesdialog.h"
#include "settings.h"
#include "musiclibrarymodel.h"
#include "albumsmodel.h"
#include "mpdparseutils.h"
#include <KDE/KAction>
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KMessageBox>
#include <KDE/KIO/FileCopyJob>
#include <KDE/KIO/Job>

enum Pages
{
    PAGE_START,
    PAGE_ERROR,
    PAGE_SKIP,
    PAGE_PROGRESS
};

static Device::NameOptions readMpdOpts()
{
    Device::NameOptions opts;
    QString scheme=Settings::self()->filenameScheme();
    if (!scheme.isEmpty()) {
        opts.scheme=scheme;
    }
    opts.vfatSafe=Settings::self()->vfatSafeFilenames();
    opts.asciiOnly=Settings::self()->asciiOnlyFilenames();
    opts.ignoreThe=Settings::self()->ignoreTheInFilenames();
    opts.replaceSpaces=Settings::self()->replaceSpacesInFilenames();
    return opts;
}

ActionDialog::ActionDialog(QWidget *parent, KAction *updateDbAct)
    : KDialog(parent)
    , propDlg(0)
    , currentDev(0)
    , updateDbAction(updateDbAct)
{
    setButtons(KDialog::Ok|KDialog::Cancel);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    errorIcon->setPixmap(QIcon::fromTheme("dialog-error").pixmap(64, 64));
    skipIcon->setPixmap(QIcon::fromTheme("dialog-warning").pixmap(64, 64));
    configureButton->setIcon(QIcon::fromTheme("configure"));
    overwrite->setChecked(Settings::self()->overwriteSongs());
    connect(configureButton, SIGNAL(pressed()), SLOT(configureDest()));
}

void ActionDialog::copy(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs)
{
    init(srcUdi, dstUdi, songs, Copy);
    // check space...
    qint64 spaceRequired=0;
    foreach (const Song &s, songsToAction) {
        if(s.size>0) {
            spaceRequired+=s.size;
        }
    }

    qint64 spaceAvailable=DevicesModel::self()->freeSpace(destUdi);

    if (spaceAvailable>spaceRequired) {
        Device *dev=DevicesModel::self()->device(sourceUdi.isEmpty() ? destUdi : sourceUdi);
        sourceLabel->setText(QLatin1String("<b>")+(sourceUdi.isEmpty() ? i18n("Local Music Library") : dev->data())+QLatin1String("</b>"));
        destinationLabel->setText(QLatin1String("<b>")+(destUdi.isEmpty() ? i18n("Local Music Library") : dev->data())+QLatin1String("</b>"));
        namingOptions=readMpdOpts();
        setPage(PAGE_START);
        mode=Copy;
        show();
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
    setPage(PAGE_PROGRESS);
    QString baseDir=udi.isEmpty() ? mpdDir : QString();
    foreach (const Song &s, songsToAction) {
        dirsToClean.insert(baseDir+MPDParseUtils::getDir(s.file));
    }
    show();
    doNext();
}

void ActionDialog::init(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs, Mode m)
{
    resize(400, 160);
    mpdDir=Settings::self()->mpdDir();
    if (!mpdDir.endsWith('/')) {
        mpdDir+QChar('/');
    }
    sourceUdi=srcUdi;
    destUdi=dstUdi;
    songsToAction=songs;
    mode=m;
    setCaption(Copy==mode ? i18n("Copy Songs") : i18n("Delete Songs"));
    qSort(songsToAction);
    progressLabel->setText(QString());
    progressBar->setValue(0);
    progressBar->setRange(0, songsToAction.count()+(Copy==mode ? 0 : 1));
    autoSkip=false;
    performingAction=false;
    paused=false;
    actionedSongs.clear();
    currentDev=0;
}

void ActionDialog::slotButtonClicked(int button)
{
    switch(stack->currentIndex()) {
    case PAGE_START:
        switch (button) {
        case KDialog::Ok:
            Settings::self()->saveOverwriteSongs(overwrite->isChecked());
            setPage(PAGE_PROGRESS);
            doNext();
            break;
        case KDialog::Cancel:
            refreshMpd();
            reject();
            break;
        default:
            KDialog::slotButtonClicked(button);
            break;
        }
        break;
    case PAGE_SKIP:
        setPage(PAGE_PROGRESS);
        switch(button) {
        case User1:
            doNext();
            break;
        case User2:
            autoSkip=true;
            doNext();
            break;
        default:
            refreshMpd();
            reject();
            break;
        }
        break;
    case PAGE_ERROR:
        refreshMpd();
        reject();
        break;
    case PAGE_PROGRESS:
        paused=true;
        if (KMessageBox::Yes==KMessageBox::questionYesNo(this, i18n("Are you sure you wish to cancel?"))) {
            refreshMpd();
            reject();
        } else if (!performingAction && PAGE_PROGRESS==stack->currentIndex()) {
            paused=false;
            doNext();
        }
    }
}

void ActionDialog::doNext()
{
    if (songsToAction.count()) {
        currentSong=songsToAction.takeFirst();
        if(Copy==mode) {
            bool copyToDev=sourceUdi.isEmpty();
            Device *dev=DevicesModel::self()->device(copyToDev ? destUdi : sourceUdi);

            if (dev) {
                if (dev!=currentDev) {
                    connect(dev, SIGNAL(actionStatus(int)), this, SLOT(actionStatus(int)));
                    currentDev=dev;
                }
                performingAction=true;
                if (copyToDev) {
                    destFile=dev->path()+dev->namingOptions().createFilename(currentSong);
                    currentSong.file=mpdDir+currentSong.file;
                    dev->addSong(currentSong, overwrite->isChecked());
                } else {
                    QString fileName=namingOptions.createFilename(currentSong);
                    destFile=mpdDir+fileName;
                    dev->copySongTo(currentSong, mpdDir, fileName, overwrite->isChecked());
                }
                progressLabel->setText(formatSong(currentSong));
            } else {
                setPage(PAGE_ERROR, i18n("Device has been removed"));
            }
        } else {
            if (sourceUdi.isEmpty()) {
                performingAction=true;
                currentSong.file=mpdDir+currentSong.file;
                removeSong(currentSong);
            } else {
                Device *dev=DevicesModel::self()->device(sourceUdi);
                if (dev) {
                    if (dev!=currentDev) {
                        connect(dev, SIGNAL(actionStatus(int)), this, SLOT(actionStatus(int)));
                        currentDev=dev;
                    }
                    performingAction=true;
                    dev->removeSong(currentSong);
                    progressLabel->setText(formatSong(currentSong));
                } else {
                    setPage(PAGE_ERROR, i18n("Device has been removed"));
                }
            }
        }
    } else if (Remove==mode && dirsToClean.count()) {
        Device *dev=sourceUdi.isEmpty() ? 0 : DevicesModel::self()->device(sourceUdi);
        QString base=sourceUdi.isEmpty() ? mpdDir : (dev ? dev->path() : QString());
        progressLabel->setText(i18n("Clearing unused folders"));
        if (!base.isEmpty()) {
            foreach (const QString &d, dirsToClean) {
                Device::cleanDir(d, base);
            }
        }
        dirsToClean.clear();
        progressBar->setValue(progressBar->value()+1);
        doNext();
    } else {
        refreshMpd();
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
        if (1==actionedSongs.count() && ( (Copy==mode && !sourceUdi.isEmpty()) ||
                                          (Remove==mode && sourceUdi.isEmpty()) ) ) {
            // Cache is now out of date, so need to remove!
            MusicLibraryModel::self()->removeCache();
        }
        progressBar->setValue(progressBar->value()+1);
        if (!paused) {
            doNext();
        }
        break;
    case Device::FileExists:
        setPage(PAGE_SKIP, i18n("The destination filename already exists!<hr/>%1", formatSong(currentSong)));
        break;
    case Device::SongExists:
        setPage(PAGE_SKIP, i18n("Song already exists!<hr/>%1", formatSong(currentSong)));
        break;
    case Device::DirCreationFaild:
        setPage(PAGE_SKIP, i18n("Failed to create destination folder!<br/>Please check you have sufficient permissions.<hr/>%1", formatSong(currentSong)));
        break;
    case Device::SourceFileDoesNotExist:
        setPage(PAGE_SKIP, i18n("Source file nolonger exists?<br/><br/<hr/>%1", formatSong(currentSong)));
        break;
    case Device::Failed:
        setPage(PAGE_SKIP, Copy==mode ? i18n("<p>Failed to copy.<hr/>%1", formatSong(currentSong))
                                      : i18n("<p>Failed to delete.<hr/>%1", formatSong(currentSong)));
        break;
    case Device::NotConnected:
        setPage(PAGE_ERROR, i18n("Lost connection to device.<hr/>%1", formatSong(currentSong)));
        break;
    default:
        break;
    }
}

void ActionDialog::configureDest()
{
    if (destUdi.isEmpty()) {
        if (!propDlg) {
            propDlg=new DevicePropertiesDialog(this);
            connect(propDlg, SIGNAL(updatedSettings(const QString &, const Device::NameOptions &)), SLOT(saveProperties(const QString &, const Device::NameOptions &)));
        }
        propDlg->show(mpdDir, QString(), readMpdOpts(), false);
    } else {
        Device *dev=DevicesModel::self()->device(destUdi);
        if (dev) {
            dev->configure(this);
        }
    }
}

void ActionDialog::saveProperties(const QString &path, const Device::NameOptions &opts)
{
    Q_UNUSED(path)
    Settings::self()->saveFilenameScheme(opts.scheme);
    Settings::self()->saveVfatSafeFilenames(opts.vfatSafe);
    Settings::self()->saveAsciiOnlyFilenames(opts.asciiOnly);
    Settings::self()->saveIgnoreTheInFilenames(opts.ignoreThe);
    Settings::self()->saveReplaceSpacesInFilenames(opts.replaceSpaces);
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
            setButtons(Cancel);
            break;
        case PAGE_SKIP:
            skipText->setText(i18n("<b>Error</b><br/>")+msg);
            skipText->setToolTip(formatSong(currentSong, true));
            setButtons(Cancel|User1|User2);
            setButtonText(User1, i18n("Skip"));
            setButtonText(User2, i18n("Auto Skip"));
            break;
        case PAGE_ERROR:
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
                       "</table>", s.artist, s.album, s.title, s.file, destFile)
                : i18n("<table>"
                       "<tr><td align=\"right\">Artist:</td><td>%1</td></tr>"
                       "<tr><td align=\"right\">Album:</td><td>%2</td></tr>"
                       "<tr><td align=\"right\">Track:</td><td>%3</td></tr>"
                       "<tr><td align=\"right\">File:</td><td>%4</td></tr>"
                       "</table>", s.artist, s.album, s.title, s.file)
            : i18n("<table>"
                   "<tr><td align=\"right\">Artist:</td><td>%1</td></tr>"
                   "<tr><td align=\"right\">Album:</td><td>%2</td></tr>"
                   "<tr><td align=\"right\">Track:</td><td>%3</td></tr>"
                   "</table>", s.artist, s.album, s.title);
}

void ActionDialog::refreshMpd()
{
    if (!actionedSongs.isEmpty() && ( (Copy==mode && !sourceUdi.isEmpty()) ||
                                      (Remove==mode && sourceUdi.isEmpty()) ) ) {
        AlbumsModel::self()->update(MusicLibraryModel::self()->root());
        updateDbAction->trigger();
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
        actionStatus(Device::Ok);
    }
}
