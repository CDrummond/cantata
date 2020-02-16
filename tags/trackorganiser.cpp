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

#include "trackorganiser.h"
#include "devices/filenameschemedialog.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "models/devicesmodel.h"
#endif
#include "devices/device.h"
#include "gui/settings.h"
#include "mpd-interface/mpdconnection.h"
#include "support/utils.h"
#include "context/songview.h"
#include "support/messagebox.h"
#include "support/action.h"
#include "widgets/icons.h"
#include "widgets/basicitemdelegate.h"
#include "mpd-interface/cuefile.h"
#include "gui/covers.h"
#include "context/contextwidget.h"
#include <QTimer>
#include <QFile>
#include <QDir>
#include <algorithm>

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

static int iCount=0;

int TrackOrganiser::instanceCount()
{
    return iCount;
}

TrackOrganiser::TrackOrganiser(QWidget *parent)
    : SongDialog(parent, "TrackOrganiser",  QSize(800, 500))
    , schemeDlg(nullptr)
    , autoSkip(false)
    , paused(false)
    , updated(false)
    , alwaysUpdate(false)
{
    iCount++;
    setButtons(Ok|Cancel);
    setCaption(tr("Organize Files"));
    setAttribute(Qt::WA_DeleteOnClose);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    configFilename->setIcon(Icons::self()->configureIcon);
    setButtonGuiItem(Ok, GuiItem(tr("Rename")));
    connect(this, SIGNAL(update()), MPDConnection::self(), SLOT(updateMaybe()));
    progress->setVisible(false);
    files->setItemDelegate(new BasicItemDelegate(files));
    files->setAlternatingRowColors(false);
    files->setContextMenuPolicy(Qt::ActionsContextMenu);
    files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    removeAct=new Action(tr("Remove From List"), files);
    removeAct->setEnabled(false);
    files->addAction(removeAct);
    connect(files, SIGNAL(itemSelectionChanged()), SLOT(controlRemoveAct()));
    connect(removeAct, SIGNAL(triggered()), SLOT(removeItems()));
}

TrackOrganiser::~TrackOrganiser()
{
    iCount--;
}

void TrackOrganiser::show(const QList<Song> &songs, const QString &udi, bool forceUpdate)
{
    // If we are called from the TagEditor dialog, then forceUpdate will be true. This is so that we dont do 2
    // MPD updates (one from TagEditor, and one from here!)
    alwaysUpdate=forceUpdate;
    for (const Song &s: songs) {
        if (!CueFile::isCue(s.file)) {
           origSongs.append(s);
        }
    }

    if (origSongs.isEmpty()) {
        deleteLater();
        if (alwaysUpdate) {
            doUpdate();
        }
        return;
    }

    QString musicFolder;
    #ifdef ENABLE_DEVICES_SUPPORT
    if (udi.isEmpty()) {
        musicFolder=MPDConnection::self()->getDetails().dir;
        opts.load(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);
    } else {
        deviceUdi=udi;
        Device *dev=getDevice(parentWidget());

        if (!dev) {
            deleteLater();
            return;
        }

        opts=dev->options();
        musicFolder=dev->path();
    }
    #else
    opts.load(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);
    musicFolder=MPDConnection::self()->getDetails().dir;
    #endif
    std::sort(origSongs.begin(), origSongs.end());

    filenameScheme->setText(opts.scheme);
    vfatSafe->setChecked(opts.vfatSafe);
    asciiOnly->setChecked(opts.asciiOnly);
    ignoreThe->setChecked(opts.ignoreThe);
    replaceSpaces->setChecked(opts.replaceSpaces);

    connect(configFilename, SIGNAL(clicked()), SLOT(configureFilenameScheme()));
    connect(filenameScheme, SIGNAL(textChanged(const QString &)), this, SLOT(updateView()));
    connect(vfatSafe, SIGNAL(toggled(bool)), this, SLOT(updateView()));
    connect(asciiOnly, SIGNAL(toggled(bool)), this, SLOT(updateView()));
    connect(ignoreThe, SIGNAL(toggled(bool)), this, SLOT(updateView()));
    connect(replaceSpaces, SIGNAL(toggled(bool)), this, SLOT(updateView()));

    if (!songsOk(origSongs, musicFolder, udi.isEmpty())) {
        return;
    }
    connect(ratingsNote, SIGNAL(leftClickedUrl()), SLOT(showRatingsMessage()));
    Dialog::show();
    enableButtonOk(false);
    updateView();
}

void TrackOrganiser::slotButtonClicked(int button)
{
    switch (button) {
    case Ok:
        startRename();
        break;
    case Cancel:
        if (!optionsBox->isEnabled()) {
            paused=true;
            if (MessageBox::No==MessageBox::questionYesNo(this, tr("Abort renaming of files?"), tr("Abort"), GuiItem(tr("Abort")), StdGuiItem::cancel())) {
                paused=false;
                QTimer::singleShot(0, this, SLOT(renameFile()));
                return;
            }
        }
        finish(false);
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}

void TrackOrganiser::configureFilenameScheme()
{
    if (!schemeDlg) {
        schemeDlg=new FilenameSchemeDialog(this);
        connect(schemeDlg, SIGNAL(scheme(const QString &)), this, SLOT(setFilenameScheme(const QString &)));
    }
    readOptions();
    schemeDlg->show(opts);
}

void TrackOrganiser::readOptions()
{
    opts.scheme=filenameScheme->text().trimmed();
    opts.vfatSafe=vfatSafe->isChecked();
    opts.asciiOnly=asciiOnly->isChecked();
    opts.ignoreThe=ignoreThe->isChecked();
    opts.replaceSpaces=replaceSpaces->isChecked();
}

void TrackOrganiser::updateView()
{
    QFont f(font());
    f.setItalic(true);
    files->clear();
    bool different=false;
    readOptions();

    QString musicFolder;
    #ifdef ENABLE_DEVICES_SUPPORT
    if (!deviceUdi.isEmpty()) {
        Device *dev=getDevice();
        if (!dev) {
            return;
        }
        musicFolder=dev->path();
    } else
    #endif
        musicFolder=MPDConnection::self()->getDetails().dir;

    for (const Song &s: origSongs) {
        QString modified=musicFolder + opts.createFilename(s);
        //different=different||(modified!=s.file);
        QString orig=s.filePath(musicFolder);
        bool diff=modified!=orig;
        different|=diff;
        QTreeWidgetItem *item=new QTreeWidgetItem(files, QStringList() << orig << modified);
        if (diff) {
            item->setFont(0, f);
            item->setFont(1, f);
        }
    }
    files->resizeColumnToContents(0);
    files->resizeColumnToContents(1);
    enableButtonOk(different);
}

void TrackOrganiser::startRename()
{
    optionsBox->setEnabled(false);
    progress->setVisible(true);
    progress->setRange(1, origSongs.count());
    enableButtonOk(false);
    index=0;
    paused=autoSkip=false;
    saveOptions();

    QTimer::singleShot(100, this, SLOT(renameFile()));
}

void TrackOrganiser::renameFile()
{
    if (paused) {
        return;
    }

    progress->setValue(progress->value()+1);

    QTreeWidgetItem *item=files->topLevelItem(index);
    files->scrollToItem(item);
    Song s=origSongs.at(index);
    QString modified=opts.createFilename(s);
    QString musicFolder;

    #ifdef ENABLE_DEVICES_SUPPORT
    if (!deviceUdi.isEmpty()) {
        Device *dev=getDevice();
        if (!dev) {
            return;
        }
        musicFolder=dev->path();
    } else
    #endif
        musicFolder=MPDConnection::self()->getDetails().dir;

    QString source=s.filePath(musicFolder);
    QString dest=musicFolder+modified;
    if (source!=dest) {
        bool skip=false;
        if (!QFile::exists(source)) {
            if (autoSkip) {
                skip=true;
            } else {
                switch(MessageBox::questionYesNoCancel(this, tr("Source file does not exist!")+QLatin1String("\n\n")+dest,
                                                       QString(), GuiItem(tr("Skip")), GuiItem(tr("Auto Skip")))) {
                case MessageBox::Yes:
                    skip=true;
                    break;
                case MessageBox::No:
                    autoSkip=skip=true;
                    break;
                case MessageBox::Cancel:
                    finish(false);
                    return;
                }
            }
        }
        // Check if dest exists...
        if (!skip && QFile::exists(dest)) {
            if (autoSkip) {
                skip=true;
            } else {
                switch(MessageBox::questionYesNoCancel(this, tr("Destination file already exists!")+QLatin1String("\n\n")+dest,
                                                       QString(), GuiItem(tr("Skip")), GuiItem(tr("Auto Skip")))) {
                case MessageBox::Yes:
                    skip=true;
                    break;
                case MessageBox::No:
                    autoSkip=skip=true;
                    break;
                case MessageBox::Cancel:
                    finish(false);
                    return;
                }
            }
        }

        // Create dest folder...
        if (!skip) {
            QDir dir(Utils::getDir(dest));
            if(!dir.exists() && !Utils::createWorldReadableDir(dir.absolutePath(), musicFolder)) {
                if (autoSkip) {
                    skip=true;
                } else {
                    switch(MessageBox::questionYesNoCancel(this, tr("Failed to create destination folder!")+QLatin1String("\n\n")+dir.absolutePath(),
                                                           QString(), GuiItem(tr("Skip")), GuiItem(tr("Auto Skip")))) {
                    case MessageBox::Yes:
                        skip=true;
                        break;
                    case MessageBox::No:
                        autoSkip=skip=true;
                        break;
                    case MessageBox::Cancel:
                        finish(false);
                        return;
                    }
                }
            }
        }

        bool renamed=false;
        if (!skip && !(renamed=QFile::rename(source, dest))) {
            if (autoSkip) {
                skip=true;
            } else {
                switch(MessageBox::questionYesNoCancel(this, tr("Failed to rename '%1' to '%2'").arg(source, dest),
                                                       QString(), GuiItem(tr("Skip")), GuiItem(tr("Auto Skip")))) {
                case MessageBox::Yes:
                    skip=true;
                    break;
                case MessageBox::No:
                    autoSkip=skip=true;
                    break;
                case MessageBox::Cancel:
                    finish(false);
                    return;
                }
            }
        }

        // If file was renamed, then also rename any other matching files...
        QDir sDir(Utils::getDir(source));
        if (renamed) {
            QFileInfoList files = sDir.entryInfoList(QStringList() << Utils::changeExtension(Utils::getFile(source), ".*"));
            for (const auto &file : files) {
                QString destFile = Utils::changeExtension(dest, "."+file.suffix());
                if (!QFile::exists(destFile)) {
                    QFile::rename(file.absoluteFilePath(), destFile);
                }
            }
        }

        if (!skip) {
            QDir sArtistDir(sDir); sArtistDir.cdUp();
            QDir dDir(Utils::getDir(dest));
            #ifdef ENABLE_DEVICES_SUPPORT
            Device *dev=deviceUdi.isEmpty() ? nullptr : getDevice();
            if (sDir.absolutePath()!=dDir.absolutePath()) {
                Device::moveDir(sDir.absolutePath(), dDir.absolutePath(), musicFolder, dev ? dev->coverFile()
                                                                                           : QString(Covers::albumFileName(s)+QLatin1String(".jpg")));
            }
            #else
            if (sDir.absolutePath()!=dDir.absolutePath()) {
                Device::moveDir(sDir.absolutePath(), dDir.absolutePath(), musicFolder,  QString(Covers::albumFileName(s)+QLatin1String(".jpg")));
            }
            #endif
            QDir dArtistDir(dDir); dArtistDir.cdUp();

            // Move any artist, or backdrop, image...
            if (sArtistDir.exists() && dArtistDir.exists() && sArtistDir.absolutePath()!=sDir.absolutePath() && sArtistDir.absolutePath()!=dArtistDir.absolutePath()) {
                QStringList artistImages;
                QFileInfoList entries=sArtistDir.entryInfoList(QDir::Files|QDir::Dirs|QDir::NoDotAndDotDot);
                QSet<QString> acceptable=QSet<QString>() << Covers::constArtistImage+QLatin1String(".jpg")
                                                         << Covers::constArtistImage+QLatin1String(".png")
                                                         << Covers::constComposerImage+QLatin1String(".jpg")
                                                         << Covers::constComposerImage+QLatin1String(".png")
                                                         << ContextWidget::constBackdropFileName+QLatin1String(".jpg")
                                                         << ContextWidget::constBackdropFileName+QLatin1String(".png");

                for (const QFileInfo &entry: entries) {
                    if (entry.isDir() || !acceptable.contains(entry.fileName())) {
                        artistImages.clear();
                        break;
                    } else {
                        artistImages.append(entry.fileName());
                    }
                }
                if (!artistImages.isEmpty()) {
                    bool delDir=true;
                    for (const QString &f: artistImages) {
                        if (!QFile::rename(sArtistDir.absolutePath()+Utils::constDirSep+f, dArtistDir.absolutePath()+Utils::constDirSep+f)) {
                            delDir=false;
                            break;
                        }
                    }
                    if (delDir) {
                        QString dirName=sArtistDir.dirName();
                        if (!dirName.isEmpty()) {
                            sArtistDir.cdUp();
                            sArtistDir.rmdir(dirName);
                        }
                    }
                }
            }
            item->setText(0, dest);
            item->setFont(0, font());
            item->setFont(1, font());
            Song to=s;
            QString origPath;
            if (s.file.startsWith(Song::constMopidyLocal)) {
                origPath=to.file;
                to.file=Song::encodePath(to.file);
            } else if (MPDConnection::self()->isForkedDaapd()) {
                to.file=Song::constForkedDaapdLocal + dest;
            } else {
                to.file=modified;
            }
            origSongs.replace(index, to);
            updated=true;

            if (deviceUdi.isEmpty()) {
//                MusicLibraryModel::self()->updateSongFile(s, to);
//                DirViewModel::self()->removeFileFromList(s.file);
//                DirViewModel::self()->addFileToList(origPath.isEmpty() ? to.file : origPath,
//                                                    origPath.isEmpty() ? QString() : to.file);
            }
            #ifdef ENABLE_DEVICES_SUPPORT
            else {
                if (!dev) {
                    return;
                }
                dev->updateSongFile(s, to);
            }
            #endif
        }
    }
    index++;
    if (index>=origSongs.count()) {
        finish(true);
    } else {
        QTimer::singleShot(100, this, SLOT(renameFile()));
    }
}

void TrackOrganiser::controlRemoveAct()
{
    removeAct->setEnabled(files->topLevelItemCount()>1 && !files->selectedItems().isEmpty());
}

void TrackOrganiser::removeItems()
{
    if (files->topLevelItemCount()<1) {
        return;
    }

    if (MessageBox::Yes==MessageBox::questionYesNo(this, tr("Remove the selected tracks from the list?"),
                                                   tr("Remove Tracks"), StdGuiItem::remove(), StdGuiItem::cancel())) {

        QList<QTreeWidgetItem *> selection=files->selectedItems();
        for (QTreeWidgetItem *item: selection) {
            int idx=files->indexOfTopLevelItem(item);
            if (idx>-1 && idx<origSongs.count()) {
                origSongs.removeAt(idx);
                delete files->takeTopLevelItem(idx);
            }
        }
    }
}

void TrackOrganiser::showRatingsMessage()
{
    MessageBox::information(this, tr("Song ratings are not stored in the song files, but within MPD's 'sticker' database.\n\n"
                                       "If you rename a file (or the folder it is within), then the rating associated with the song will be lost."),
                            QLatin1String("Ratings"));
}

void TrackOrganiser::setFilenameScheme(const QString &text)
{
    if (filenameScheme->text()!=text) {
        filenameScheme->setText(text);
        saveOptions();
    }
}

void TrackOrganiser::saveOptions()
{
    readOptions();
    #ifdef ENABLE_DEVICES_SUPPORT
    if (!deviceUdi.isEmpty()) {
        Device *dev=getDevice();
        if (!dev) {
            return;
        }
        dev->setOptions(opts);
    } else
    #endif
    opts.save(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);
}

void TrackOrganiser::doUpdate()
{
    if (deviceUdi.isEmpty()) {
        emit update();
    }
    #ifdef ENABLE_DEVICES_SUPPORT
    else {
        Device *dev=getDevice();
        if (dev) {
            dev->saveCache();
        }
    }
    #endif
}

void TrackOrganiser::finish(bool ok)
{
    if (updated || alwaysUpdate) {
        doUpdate();
    }
    if (ok) {
        accept();
    } else {
        reject();
    }
}

#ifdef ENABLE_DEVICES_SUPPORT
Device * TrackOrganiser::getDevice(QWidget *p)
{
    Device *dev=DevicesModel::self()->device(deviceUdi);
    if (!dev) {
        MessageBox::error(p ? p : this, tr("Device has been removed!"));
        reject();
        return nullptr;
    }
    if (!dev->isConnected()) {
        MessageBox::error(p ? p : this, tr("Device is not connected."));
        reject();
        return nullptr;
    }
    if (!dev->isIdle()) {
        MessageBox::error(p ? p : this, tr("Device is busy?"));
        reject();
        return nullptr;
    }
    return dev;
}
#endif

#include "moc_trackorganiser.cpp"
