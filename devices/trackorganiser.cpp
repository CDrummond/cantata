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

#include "trackorganiser.h"
#include "filenameschemedialog.h"
#include "devicesmodel.h"
#include "musiclibrarymodel.h"
#include "settings.h"
#include "mpdconnection.h"
#include <KDE/KGlobal>
#include <KDE/KLocale>
#include <KDE/KMessageBox>
#include <QtCore/QTimer>
#include <QtCore/QDebug>

TrackOrganiser::TrackOrganiser(QWidget *parent)
    : KDialog(parent)
    , schemeDlg(0)
{
    updated=false;
    setButtons(KDialog::Ok|KDialog::Cancel);
    setCaption(i18n("Track Organizer"));
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowModality(Qt::WindowModal);
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);
    configFilename->setIcon(QIcon::fromTheme("configure"));
    setButtonGuiItem(Ok, KGuiItem(i18n("Rename"), "edit-rename"));
    connect(this, SIGNAL(getStats()), MPDConnection::self(), SLOT(getStats()));
}

void TrackOrganiser::show(const QList<Song> &songs, const QString &udi)
{
    if (udi.isEmpty()) {
        opts.load("mpd");
        origSongs=songs;
    } else {
        deviceUdi=udi;
        Device *dev=getDevice(parentWidget());

        if (!dev) {
            deleteLater();
            return;
        }

        deviceUdi=udi;
        opts=dev->options();
        int pathLen=dev->path().length();

        foreach (const Song &s, songs) {
            Song m=s;
            m.file=m.file.mid(pathLen);
            origSongs.append(m);
        }
    }
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
    KDialog::show();
    enableButtonOk(false);
    updateView();
}

void TrackOrganiser::slotButtonClicked(int button)
{
    switch (button) {
    case KDialog::Ok:
        startRename();
        break;
    case KDialog::Cancel:
        if (!optionsBox->isEnabled()) {
            paused=true;
            if (KMessageBox::No==KMessageBox::questionYesNo(this, i18n("Cancel renaming of files?"))) {
                paused=false;
                QTimer::singleShot(0, this, SLOT(renameFile()));
                return;
            }
        }
        finish(false);
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
    enableButtonOk(false);
    index=0;
    paused=autoSkip=false;
    readOptions();
    if (!deviceUdi.isEmpty()) {
        Device *dev=getDevice();
        if (!dev) {
            return;
        }
        dev->setOptions(opts);
    } else {
        opts.save("mpd");
    }
    QTimer::singleShot(100, this, SLOT(renameFile()));
}

void TrackOrganiser::renameFile()
{
    if (paused) {
        return;
    }

    QTreeWidgetItem *item=files->topLevelItem(index);
    files->scrollToItem(item);
    Song s=origSongs.at(index);
    QString modified=opts.createFilename(s);
    QString musicFolder;

    if (!deviceUdi.isEmpty()) {
        Device *dev=getDevice();
        if (!dev) {
            return;
        }
        musicFolder=dev->path();
    } else {
        musicFolder=Settings::self()->mpdDir();
    }

    if (modified!=s.file) {
        QString source=musicFolder+s.file;
        QString dest=musicFolder+modified;
        bool skip=false;
        // Check if dest exists...
        if (QFile::exists(dest)) {
            if (autoSkip) {
                skip=true;
            } else {
                switch(KMessageBox::questionYesNoCancel(this, i18n("Destination file already exists!<br/>%1", dest),
                                                        QString(), KGuiItem(i18n("Skip")), KGuiItem("Auto Skip"))) {
                case KMessageBox::Yes:
                    skip=true;
                    break;
                case KMessageBox::No:
                    autoSkip=skip=true;
                    break;
                case KMessageBox::Cancel:
                    finish(false);
                    return;
                }
            }
        }

        // Create dest folder...
        if (!skip) {
            KUrl d(dest);
            QDir dir(d.directory());
            if(!dir.exists() && !Device::createDir(dir.absolutePath())) {
                if (autoSkip) {
                    skip=true;
                } else {
                    switch(KMessageBox::questionYesNoCancel(this, i18n("Failed to create destination folder!<br/>%1", dir.absolutePath()),
                                                            QString(), KGuiItem(i18n("Skip")), KGuiItem("Auto Skip"))) {
                    case KMessageBox::Yes:
                        skip=true;
                        break;
                    case KMessageBox::No:
                        autoSkip=skip=true;
                        break;
                    case KMessageBox::Cancel:
                        finish(false);
                        return;
                    }
                }
            }
        }

        if (!skip && !QFile::rename(source, dest)) {
            if (autoSkip) {
                skip=true;
            } else {
                switch(KMessageBox::questionYesNoCancel(this, i18n("Failed to copy %1 to %2", source, dest),
                                                        QString(), KGuiItem(i18n("Skip")), KGuiItem("Auto Skip"))) {
                case KMessageBox::Yes:
                    skip=true;
                    break;
                case KMessageBox::No:
                    autoSkip=skip=true;
                    break;
                case KMessageBox::Cancel:
                    finish(false);
                    return;
                }
            }
        }

        if (!skip) {
            KUrl su(source);
            KUrl du(dest);
            QDir sDir(su.directory());
            QDir dDir(du.directory());
            Device *dev=deviceUdi.isEmpty() ? 0 : getDevice();
            if (sDir.absolutePath()!=dDir.absolutePath()) {
                Device::moveDir(sDir.absolutePath(), dDir.absolutePath(), musicFolder, dev ? dev->coverFile() : QString());
            }
            item->setText(0, modified);
            Song to=s;
            to.file=modified;
            origSongs.replace(index, to);
            updated=true;

            if (deviceUdi.isEmpty()) {
                MusicLibraryModel::self()->updateSongFile(s, to);
            } else {
                s.file=source;
                to.file=dest;
                if (!dev) {
                    return;
                }
                dev->updateSongFile(s, to);
            }
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
    if (updated && deviceUdi.isEmpty()) {
        emit getStats();
    }
    if (ok) {
        accept();
    } else {
        reject();
    }
}

Device * TrackOrganiser::getDevice(QWidget *p)
{
    Device *dev=DevicesModel::self()->device(deviceUdi);
    if (!dev) {
        KMessageBox::error(p ? p : this, i18n("Device has been removed!"));
        reject();
        return 0;
    }
    if (!dev->isIdle()) {
        KMessageBox::error(p ? p : this, i18n("Device is busy?"));
        reject();
        return 0;
    }
    return dev;
}
