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

#include "syncdialog.h"
#include "synccollectionwidget.h"
#include "actiondialog.h"
#include "devicepropertiesdialog.h"
#include "devicepropertieswidget.h"
#include "mpd-interface/song.h"
#include "mpd-interface/mpdconnection.h"
#include "models/mpdlibrarymodel.h"
#include "models/devicesmodel.h"
#include "support/messagebox.h"
#include "support/squeezedtextlabel.h"
#include "widgets/icons.h"
#include <QSplitter>
#include <QFileInfo>

struct SyncSong : public Song
{
    SyncSong(const Song &o)
        : Song(o) {
    }
    bool operator==(const SyncSong &o) const {
        return title==o.title && album==o.album && albumArtist()==o.albumArtist();
    }
    bool operator<(const SyncSong &o) const {
        int compare=albumArtist().compare(o.albumArtist());

        if (0!=compare) {
            return compare<0;
        }
        compare=album.compare(o.album);
        if (0!=compare) {
            return compare<0;
        }
        return title.compare(o.title)<0;
    }
};

inline uint qHash(const SyncSong &key)
{
    return qHash(key.albumArtist()+key.artist+key.title);
}

static void getDiffs(const QSet<Song> &s1, const QSet<Song> &s2, QSet<Song> &in1, QSet<Song> &in2)
{
    QSet<SyncSong> a;
    QSet<SyncSong> b;

    for (const Song &s: s1) {
        a.insert(s);
    }

    for (const Song &s: s2) {
        b.insert(s);
    }

    QSet<SyncSong> r=a-b;

    for (const Song &s: r) {
        in1.insert(s);
    }

    r=b-a;

    for (const Song &s: r) {
        in2.insert(s);
    }
}

static int iCount=0;

int SyncDialog::instanceCount()
{
    return iCount;
}

SyncDialog::SyncDialog(QWidget *parent)
    : Dialog(parent, "SyncDialog", QSize(680, 680))
    , state(State_Lists)
    , currentDev(nullptr)
{
    iCount++;

    QWidget *mw=new QWidget(this);
    QVBoxLayout *l=new QVBoxLayout(mw);
    QSplitter *splitter=new QSplitter(mw);
    libWidget=new SyncCollectionWidget(splitter, tr("Library:"));
    devWidget=new SyncCollectionWidget(splitter, tr("Device:"));
    statusLabel=new SqueezedTextLabel(this);
    statusLabel->setText(tr("Loading all songs from library, please wait..."));
    splitter->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
    NoteLabel *noteLabel=new NoteLabel(this);
    noteLabel->setText(tr("<code>Library</code> lists only songs that are in your library, but not on the device. Likewise <code>Device</code> lists "
                            "songs that are only on the device.<br/>"
                            "Select songs from <code>Library</code> that you would like to copy to <code>Device</code>, "
                            "and select songs from <code>Device</code> that you would like to copy to <code>Library</code>. "
                            "Then press the <code>Synchronize</code> button."));
    l->addWidget(splitter);
    l->addWidget(statusLabel);
    l->addWidget(noteLabel);
    libWidget->setEnabled(false);
    devWidget->setEnabled(false);
    setMainWidget(mw);
    setButtons(Cancel|Ok);
    setButtonText(Ok, tr("Synchronize"));
    enableButtonOk(false);
    setAttribute(Qt::WA_DeleteOnClose);
    setCaption(tr("Synchronize"));
    connect(libWidget, SIGNAL(selectionChanged()), SLOT(selectionChanged()));
    connect(devWidget, SIGNAL(selectionChanged()), SLOT(selectionChanged()));
    connect(libWidget, SIGNAL(configure()), SLOT(configure()));
    connect(devWidget, SIGNAL(configure()), SLOT(configure()));
    libOptions.save(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true);
}

SyncDialog::~SyncDialog()
{
    iCount--;
}

void SyncDialog::sync(const QString &udi)
{
    devUdi=udi;
    connect(MpdLibraryModel::self(), SIGNAL(songListing(QList<Song>,double)), this, SLOT(librarySongs(QList<Song>,double)));
    MpdLibraryModel::self()->listSongs();
    show();
}

void SyncDialog::copy(const QList<Song> &songs)
{
    Device *dev=getDevice();

    if (!dev) {
        return;
    }

    bool fromDev=sender()==devWidget;
    ActionDialog *dlg=new ActionDialog(this);
    connect(dlg, SIGNAL(completed()), SLOT(updateSongs()));
    dlg->copy(fromDev ? dev->id() : QString(), fromDev ? QString() : dev->id(), songs);
}

void SyncDialog::updateSongs()
{
    Device *dev=getDevice();

    if (!dev) {
        deleteLater();
        hide();
        return;
    }

    QSet<Song> devSongs=dev->allSongs(dev->options().fixVariousArtists);
    QSet<Song> inDev;
    QSet<Song> inLib;
    getDiffs(devSongs, libSongs, inDev, inLib);

    if (0==inDev.count() && 0==inLib.count()) {
        MessageBox::information(isVisible() ? this : parentWidget(), tr("Device and library are in sync."));
        deleteLater();
        hide();
        return;
    }
    devWidget->setSupportsAlbumArtistTag(dev->supportsAlbumArtistTag());
    devWidget->update(inDev);
    libWidget->update(inLib);
    libWidget->setEnabled(true);
    devWidget->setEnabled(true);
    libSongs.clear();
}

void SyncDialog::librarySongs(const QList<Song> &songs, double pc)
{
    if (songs.isEmpty()) {
        statusLabel->hide();
        disconnect(MpdLibraryModel::self(), SIGNAL(songListing(QList<Song>,double)), this, SLOT(librarySongs(QList<Song>,double)));
        updateSongs();
    } else {
        libSongs+=songs.toSet();
        statusLabel->setText(tr("Loading all songs from library, please wait...%1%...").arg(pc));
    }
}

void SyncDialog::selectionChanged()
{
    enableButtonOk(libWidget->numCheckedSongs() || devWidget->numCheckedSongs());
}

void SyncDialog::configure()
{
    if (libWidget==sender()) {
        DevicePropertiesDialog *dlg=new DevicePropertiesDialog(this);
        connect(dlg, SIGNAL(updatedSettings(const QString &, const DeviceOptions &)), SLOT(saveProperties(const QString &, const DeviceOptions &)));
        dlg->setCaption(tr("Local Music Library Properties"));
        dlg->show(MPDConnection::self()->getDetails().dir, libOptions, DevicePropertiesWidget::Prop_Basic|DevicePropertiesWidget::Prop_FileName);
    } else {
        Device *dev=getDevice();
        if (dev) {
            dev->configure(this);
        }
    }
}

void SyncDialog::saveProperties(const QString &path, const DeviceOptions &opts)
{
    Q_UNUSED(path)
    libOptions=opts;
    libOptions.save(MPDConnectionDetails::configGroupName(MPDConnection::self()->getDetails().name), true, false);
}

void SyncDialog::slotButtonClicked(int button)
{
    switch(button) {
    case Ok: {
        Device *dev=getDevice();
        if (dev) {
            setVisible(false);
            ActionDialog *dlg=new ActionDialog(parentWidget());
            QList<Song> songs=libWidget->checkedSongs();
            QString devId;
            devId=dev->id();
            dlg->sync(devId, libWidget->checkedSongs(), devWidget->checkedSongs());
            Dialog::slotButtonClicked(button);
        }
        break;
    }
    case Cancel:
        MpdLibraryModel::self()->cancelListing();
        Dialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}

Device * SyncDialog::getDevice()
{
    Device *dev=DevicesModel::self()->device(devUdi);
    if (!dev) {
        MessageBox::error(isVisible() ? this : parentWidget(), tr("Device has been removed!"));
        return nullptr;
    }

    if (currentDev && dev!=currentDev) {
        MessageBox::error(isVisible() ? this : parentWidget(), tr("Device has been changed?"));
        return nullptr;
    }

    if (dev->isIdle()) {
        return dev;
    }

    MessageBox::error(isVisible() ? this : parentWidget(), tr("Device is busy?"));
    return nullptr;
}

#include "moc_syncdialog.cpp"
