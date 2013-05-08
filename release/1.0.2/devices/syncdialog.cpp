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

#include "syncdialog.h"
#include "synccollectionwidget.h"
#include "actiondialog.h"
#include "song.h"
#include "musiclibrarymodel.h"
#include "devicesmodel.h"
#include "localize.h"
#include "messagebox.h"
#include <QSplitter>

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

    foreach (const Song &s, s1) {
        a.insert(s);
    }

    foreach (const Song &s, s2) {
        b.insert(s);
    }

    QSet<SyncSong> r=a-b;

    foreach (const Song &s, r) {
        in1.insert(s);
    }

    r=b-a;

    foreach (const Song &s, r) {
        in2.insert(s);
    }
}

static int iCount=0;

int SyncDialog::instanceCount()
{
    return iCount;
}

SyncDialog::SyncDialog(QWidget *parent)
    : Dialog(parent)
    , currentDev(0)
{
    iCount++;

    QSplitter *splitter=new QSplitter(this);
    libWidget=new SyncCollectionWidget(splitter, i18n("Songs Only In Library:"), i18n("Copy To Device"), false);
    devWidget=new SyncCollectionWidget(splitter, i18n("Songs Only On Device:"), i18n("Copy To Library"), false);
    setMainWidget(splitter);
    setButtons(Close);
    setAttribute(Qt::WA_DeleteOnClose);
    setCaption(i18n("Synchronize"));
    connect(libWidget, SIGNAL(copy(const QList<Song> &)), SLOT(copy(const QList<Song> &)));
    connect(devWidget, SIGNAL(copy(const QList<Song> &)), SLOT(copy(const QList<Song> &)));
}

SyncDialog::~SyncDialog()
{
    iCount--;
}

void SyncDialog::sync(const QString &udi)
{
    devUdi=udi;
    if (updateSongs(true)) {
        int numArtists=qMax(devWidget->numArtists(), libWidget->numArtists());
        int listSize=QFontMetrics(font()).height()*numArtists;
        int size=140+listSize;
        resize(680, size=size<300 ? 300 : (size>680 ? 680 : size));
        show();
    }
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
    dlg->copy(fromDev ? dev->udi() : QString(), fromDev ? QString() : dev->udi(), songs);
}

bool SyncDialog::updateSongs(bool firstRun)
{
    Device *dev=getDevice();

    if (!dev) {
        deleteLater();
        hide();
        return false;
    }

    QSet<Song> devSongs=dev->allSongs(dev->options().fixVariousArtists);
    QSet<Song> libSongs=MusicLibraryModel::self()->root()->allSongs();
    QSet<Song> inDev;
    QSet<Song> inLib;
    getDiffs(devSongs, libSongs, inDev, inLib);

    if (0==inDev.count() && 0==inLib.count()) {
        if (firstRun) {
            MessageBox::information(isVisible() ? this : parentWidget(), i18n("Device and library are in sync."));
        }
        deleteLater();
        hide();
        return false;
    }
    if (firstRun) {
        devWidget->setSupportsAlbumArtistTag(dev->supportsAlbumArtistTag());
    }
    devWidget->update(inDev);
    libWidget->update(inLib);
    return true;
}

Device * SyncDialog::getDevice()
{
    Device *dev=DevicesModel::self()->device(devUdi);
    if (!dev) {
        MessageBox::error(isVisible() ? this : parentWidget(), i18n("Device has been removed!"));
        return 0;
    }

    if (currentDev && dev!=currentDev) {
        MessageBox::error(isVisible() ? this : parentWidget(), i18n("Device has been changed?"));
        return 0;
    }

    if (dev->isIdle()) {
        return dev;
    }

    MessageBox::error(isVisible() ? this : parentWidget(), i18n("Device is busy?"));
    return 0;
}
