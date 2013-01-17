/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "filenameschemedialog.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "devicesmodel.h"
#endif
#include "device.h"
#include "musiclibrarymodel.h"
#include "dirviewmodel.h"
#include "settings.h"
#include "mpdconnection.h"
#include "utils.h"
#include "lyricspage.h"
#include "localize.h"
#include "messagebox.h"
#include "icons.h"
#include <QtCore/QTimer>
#include <QtCore/QFile>
#include <QtCore/QDir>

static int iCount=0;

int TrackOrganiser::instanceCount()
{
    return iCount;
}

TrackOrganiser::TrackOrganiser(QWidget *parent)
    : Dialog(parent)
    , schemeDlg(0)
{
    iCount++;
    updated=false;
    setButtons(Ok|Cancel);
    setCaption(i18n("Organize Files"));
    setAttribute(Qt::WA_DeleteOnClose);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    configFilename->setIcon(Icons::configureIcon);
    setButtonGuiItem(Ok, GuiItem(i18n("Rename"), "edit-rename"));
    connect(this, SIGNAL(update()), MPDConnection::self(), SLOT(update()));
    progress->setVisible(false);
}

TrackOrganiser::~TrackOrganiser()
{
    iCount--;
}

void TrackOrganiser::show(const QList<Song> &songs, const QString &udi)
{
    Q_UNUSED(udi)

    origSongs=songs;
    #ifdef ENABLE_DEVICES_SUPPORT
    if (udi.isEmpty()) {
        opts.load(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);
    } else {
        deviceUdi=udi;
        Device *dev=getDevice(parentWidget());

        if (!dev) {
            deleteLater();
            return;
        }

        opts=dev->options();
    }
    #else
    opts.load(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);
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

    resize(800, 500);
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
            if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Cancel renaming of files?"))) {
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
        connect(schemeDlg, SIGNAL(scheme(const QString &)), filenameScheme, SLOT(setText(const QString &)));
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
        bool diff=modified!=s.file;
        different|=diff;
        QTreeWidgetItem *item=new QTreeWidgetItem(files, QStringList() << s.file << modified);
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
    progress->setRange(0, origSongs.count());
    enableButtonOk(false);
    index=0;
    paused=autoSkip=false;
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

    if (modified!=s.file) {
        QString source=musicFolder+s.file;
        QString dest=musicFolder+modified;
        bool skip=false;
        // Check if dest exists...
        if (QFile::exists(dest)) {
            if (autoSkip) {
                skip=true;
            } else {
                switch(MessageBox::questionYesNoCancel(this, i18n("Destination file already exists!<br/>%1").arg(dest),
                                                       QString(), GuiItem(i18n("Skip")), GuiItem("Auto Skip"))) {
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
            if(!dir.exists() && !Utils::createDir(dir.absolutePath(), musicFolder)) {
                if (autoSkip) {
                    skip=true;
                } else {
                    switch(MessageBox::questionYesNoCancel(this, i18n("Failed to create destination folder!<br/>%1").arg(dir.absolutePath()),
                                                           QString(), GuiItem(i18n("Skip")), GuiItem("Auto Skip"))) {
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
                switch(MessageBox::questionYesNoCancel(this, i18n("Failed to rename %1 to %2").arg(source).arg(dest),
                                                       QString(), GuiItem(i18n("Skip")), GuiItem("Auto Skip"))) {
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
            QString lyricsSource=Utils::changeExtension(source, LyricsPage::constExtension);
            QString lyricsDest=Utils::changeExtension(dest, LyricsPage::constExtension);

            if (QFile::exists(lyricsSource) && !QFile::exists(lyricsDest)) {
                QFile::rename(lyricsSource, lyricsDest);
            }
        }

        if (!skip) {
            QDir sDir(Utils::getDir(source));
            QDir dDir(Utils::getDir(dest));
            #ifdef ENABLE_DEVICES_SUPPORT
            Device *dev=deviceUdi.isEmpty() ? 0 : getDevice();
            if (sDir.absolutePath()!=dDir.absolutePath()) {
                Device::moveDir(sDir.absolutePath(), dDir.absolutePath(), musicFolder, dev ? dev->coverFile() : QString());
            }
            #else
            if (sDir.absolutePath()!=dDir.absolutePath()) {
                Device::moveDir(sDir.absolutePath(), dDir.absolutePath(), musicFolder, QString());
            }
            #endif
            item->setText(0, modified);
            item->setFont(0, font());
            item->setFont(1, font());
            Song to=s;
            to.file=modified;
            origSongs.replace(index, to);
            updated=true;

            if (deviceUdi.isEmpty()) {
                MusicLibraryModel::self()->updateSongFile(s, to);
                DirViewModel::self()->removeFileFromList(s.file);
                DirViewModel::self()->addFileToList(to.file);
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

void TrackOrganiser::finish(bool ok)
{
    if (updated) {
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

