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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "trackorganiser.h"
#include "devices/filenameschemedialog.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "models/devicesmodel.h"
#endif
#include "devices/device.h"
#include "models/musiclibrarymodel.h"
#include "models/dirviewmodel.h"
#include "gui/settings.h"
#include "mpd/mpdconnection.h"
#include "support/utils.h"
#include "context/songview.h"
#include "support/localize.h"
#include "support/messagebox.h"
#include "widgets/icons.h"
#include "widgets/basicitemdelegate.h"
#include "mpd/cuefile.h"
#include "gui/covers.h"
#include "context/contextwidget.h"
#include "support/flickcharm.h"
#include <QTimer>
#include <QFile>
#include <QDir>

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
    , schemeDlg(0)
    , autoSkip(false)
    , paused(false)
    , updated(false)
    , alwaysUpdate(false)
{
    iCount++;
    setButtons(Ok|Cancel);
    setCaption(i18n("Organize Files"));
    setAttribute(Qt::WA_DeleteOnClose);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    configFilename->setIcon(Icons::self()->configureIcon);
    setButtonGuiItem(Ok, GuiItem(i18n("Rename"), "edit-rename"));
    connect(this, SIGNAL(update()), MPDConnection::self(), SLOT(update()));
    progress->setVisible(false);
    files->setItemDelegate(new BasicItemDelegate(files));
    files->setAlternatingRowColors(false);
    files->setContextMenuPolicy(Qt::ActionsContextMenu);
    files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    removeAct=new Action(i18n("Remove From List"), files);
    removeAct->setEnabled(false);
    files->addAction(removeAct);
    connect(files, SIGNAL(itemSelectionChanged()), SLOT(controlRemoveAct()));
    connect(removeAct, SIGNAL(triggered(bool)), SLOT(removeItems()));
    FlickCharm::self()->activateOn(files);
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
    foreach (const Song &s, songs) {
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
    bool isMopidy=false;
    #ifdef ENABLE_DEVICES_SUPPORT
    if (udi.isEmpty()) {
        musicFolder=MPDConnection::self()->getDetails().dir;
        opts.load(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);
        isMopidy=MPDConnection::self()->isMopdidy();
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
    isMopidy=MPDConnection::self()->isMopdidy();
    #endif
    qSort(origSongs);

    filenameScheme->setText(opts.scheme);
    vfatSafe->setChecked(opts.vfatSafe);
    asciiOnly->setChecked(opts.asciiOnly);
    ignoreThe->setChecked(opts.ignoreThe);
    replaceSpaces->setChecked(opts.replaceSpaces);

    connect(configFilename, SIGNAL(clicked()), SLOT(configureFilenameScheme()));
    connect(filenameScheme, SIGNAL(textChanged(const QString &)), this, SLOT(updateView()));
    connect(vfatSafe, SIGNAL(stateChanged(int)), this, SLOT(updateView()));
    connect(asciiOnly, SIGNAL(stateChanged(int)), this, SLOT(updateView()));
    connect(ignoreThe, SIGNAL(stateChanged(int)), this, SLOT(updateView()));

    if (!songsOk(origSongs, musicFolder, udi.isEmpty())) {
        return;
    }
    if (isMopidy) {
        connect(mopidyNote, SIGNAL(leftClickedUrl()), SLOT(showMopidyMessage()));
    } else {
        REMOVE(mopidyNote);
    }
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
            if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Abort renaming of files?"), i18n("Abort"), GuiItem(i18n("Abort")), StdGuiItem::cancel())) {
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
    foreach (const Song &s, origSongs) {
        QString modified=opts.createFilename(s);
        //different=different||(modified!=s.file);
        QString orig=s.filePath();
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

    QString orig=s.filePath();
    if (modified!=orig) {
        QString source=musicFolder+orig;
        QString dest=musicFolder+modified;
        bool skip=false;
        if (!QFile::exists(source)) {
            if (autoSkip) {
                skip=true;
            } else {
                switch(MessageBox::questionYesNoCancel(this, i18n("Source file does not exist!<br/>%1", dest),
                                                       QString(), GuiItem(i18n("Skip")), GuiItem(i18n("Auto Skip")))) {
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
                switch(MessageBox::questionYesNoCancel(this, i18n("Destination file already exists!<br/>%1", dest),
                                                       QString(), GuiItem(i18n("Skip")), GuiItem(i18n("Auto Skip")))) {
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
                    switch(MessageBox::questionYesNoCancel(this, i18n("Failed to create destination folder!<br/>%1", dir.absolutePath()),
                                                           QString(), GuiItem(i18n("Skip")), GuiItem(i18n("Auto Skip")))) {
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
                switch(MessageBox::questionYesNoCancel(this, i18n("Failed to rename %1 to %2", source, dest),
                                                       QString(), GuiItem(i18n("Skip")), GuiItem(i18n("Auto Skip")))) {
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

        // If file was renamed, then also rename any lyrics file...
        if (renamed) {
            QString lyricsSource=Utils::changeExtension(source, SongView::constExtension);
            QString lyricsDest=Utils::changeExtension(dest, SongView::constExtension);

            if (QFile::exists(lyricsSource) && !QFile::exists(lyricsDest)) {
                QFile::rename(lyricsSource, lyricsDest);
            }
        }

        if (!skip) {
            QDir sDir(Utils::getDir(source));
            QDir sArtistDir(sDir); sArtistDir.cdUp();
            QDir dDir(Utils::getDir(dest));
            #ifdef ENABLE_DEVICES_SUPPORT
            Device *dev=deviceUdi.isEmpty() ? 0 : getDevice();
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
                QString artistImage=Covers::artistFileName(s);
                QSet<QString> acceptable=QSet<QString>() << artistImage+QLatin1String(".jpg") << artistImage+QLatin1String(".png")
                                                         << ContextWidget::constBackdropFileName+QLatin1String(".jpg")
                                                         << ContextWidget::constBackdropFileName+QLatin1String(".png");

                foreach (const QFileInfo &entry, entries) {
                    if (entry.isDir() || !acceptable.contains(entry.fileName())) {
                        artistImages.clear();
                        break;
                    } else {
                        artistImages.append(entry.fileName());
                    }
                }
                if (!artistImages.isEmpty()) {
                    bool delDir=true;
                    foreach (const QString &f, artistImages) {
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
            item->setText(0, modified);
            item->setFont(0, font());
            item->setFont(1, font());
            Song to=s;
            QString origPath;
            if (s.file.startsWith(Song::constMopidyLocal)) {
                origPath=to.file;
                to.file=Song::encodePath(to.file);
            } else {
                to.file=modified;
            }
            origSongs.replace(index, to);
            updated=true;

            if (deviceUdi.isEmpty()) {
                MusicLibraryModel::self()->updateSongFile(s, to);
                DirViewModel::self()->removeFileFromList(s.file);
                DirViewModel::self()->addFileToList(origPath.isEmpty() ? to.file : origPath,
                                                    origPath.isEmpty() ? QString() : to.file);
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

    if (MessageBox::Yes==MessageBox::questionYesNo(this, i18n("Remove the selected tracks from the list?"),
                                                   i18n("Remove Tracks"), StdGuiItem::remove(), StdGuiItem::cancel())) {

        QList<QTreeWidgetItem *> selection=files->selectedItems();
        foreach (QTreeWidgetItem *item, selection) {
            int idx=files->indexOfTopLevelItem(item);
            if (idx>-1 && idx<origSongs.count()) {
                origSongs.removeAt(idx);
                delete files->takeTopLevelItem(idx);
            }
        }
    }
}

void TrackOrganiser::showMopidyMessage()
{
    MessageBox::information(this, i18n("Cantata has detected that you are connected to a Mopidy server.\n\n"
                                       "Currently it is not possible for Cantata to force Mopidy to refresh its local "
                                       "music listing. Therefore, you will need to stop Cantata, manually refresh "
                                       "Mopidy's database, and restart Cantata for any changes to be active."),
                            QLatin1String("Mopidy"));
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
        MessageBox::error(p ? p : this, i18n("Device has been removed!"));
        reject();
        return 0;
    }
    if (!dev->isConnected()) {
        MessageBox::error(p ? p : this, i18n("Device is not connected."));
        reject();
        return 0;
    }
    if (!dev->isIdle()) {
        MessageBox::error(p ? p : this, i18n("Device is busy?"));
        reject();
        return 0;
    }
    return dev;
}
#endif

