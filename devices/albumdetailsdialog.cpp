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
 * General Public License for more detailexampleSong.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "albumdetailsdialog.h"
#include "localize.h"
#include "audiocddevice.h"
#include "musiclibrarymodel.h"
#include "musiclibraryitemsong.h"
#include "messagebox.h"
#include "inputdialog.h"
#include "devicesmodel.h"
#include "cddb.h"
#include "icons.h"
#include "coverdialog.h"
#include <QMenu>
#include <QItemDelegate>
#include <QMouseEvent>

enum Columns {
    COL_TRACK,
    COL_ARTIST,
    COL_TITLE
};

class EditorDelegate : public QItemDelegate
{
public:
    EditorDelegate(QObject *parent=0) : QItemDelegate(parent) { }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        Q_UNUSED(option);
        if (COL_TRACK==index.column()) {
            SpinBox *editor = new SpinBox(parent);
            editor->setMinimum(0);
            editor->setMaximum(500);
            return editor;
        } else {
            return new QLineEdit(parent);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
        return QItemDelegate::sizeHint(option, index)+QSize(0, 4);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const {
        if (COL_TRACK==index.column()) {
            static_cast<SpinBox*>(editor)->setValue(index.model()->data(index, Qt::EditRole).toInt());
        } else {
            static_cast<QLineEdit*>(editor)->setText(index.model()->data(index, Qt::EditRole).toString());
        }
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
        if (COL_TRACK==index.column()) {
            model->setData(index, static_cast<SpinBox*>(editor)->value(), Qt::EditRole);
        } else {
            model->setData(index, static_cast<QLineEdit*>(editor)->text().trimmed(), Qt::EditRole);
        }
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        Q_UNUSED(index);
        editor->setGeometry(option.rect);
    }
};

static int iCount=0;

int AlbumDetailsDialog::instanceCount()
{
    return iCount;
}

AlbumDetailsDialog::AlbumDetailsDialog(QWidget *parent)
    : Dialog(parent)
    , pressed(false)
{
    iCount++;
    setButtons(User1|Ok|Cancel);
    setCaption(i18n("Disc Details"));
    setAttribute(Qt::WA_DeleteOnClose);

    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);

    QSet<QString> artists;
    QSet<QString> albumArtists;
    QSet<QString> albums;
    QSet<QString> genres;
    MusicLibraryModel::self()->getDetails(artists, albumArtists, albums, genres);

    QStringList strings=albumArtists.toList();
    strings.sort();
    artist->clear();
    artist->insertItems(0, strings);

    strings=albums.toList();
    strings.sort();
    title->clear();
    title->insertItems(0, strings);

    strings=genres.toList();
    strings.sort();
    genre->clear();
    genre->insertItems(0, strings);

    QMenu *toolsMenu=new QMenu(this);
    toolsMenu->addAction(i18n("Apply \"Various Artists\" Workaround"), this, SLOT(applyVa()));
    toolsMenu->addAction(i18n("Revert \"Various Artists\" Workaround"), this, SLOT(revertVa()));
    toolsMenu->addAction(i18n("Capitalize"), this, SLOT(capitalise()));
    toolsMenu->addAction(i18n("Adjust Track Numbers"), this, SLOT(adjustTrackNumbers()));
    setButtonMenu(User1, toolsMenu, InstantPopup);

    #ifdef ENABLE_KDE_SUPPORT
    setButtonGuiItem(User1, GuiItem(i18n("Tools"), "tools-wizard"));
    #else
    setButtonGuiItem(User1, GuiItem(i18n("Tools"), "tools-wizard"));
    #endif
    year->setAllowEmpty();
    disc->setAllowEmpty();
    connect(singleArtist, SIGNAL(toggled(bool)), SLOT(hideArtistColumn(bool)));
    tracks->setItemDelegate(new EditorDelegate);
    resize(600, 600);

    int size=fontMetrics().height()*5;
    cover->setMinimumSize(size, size);
    cover->setMaximumSize(size, size);
    setCover();
    cover->installEventFilter(this);
}

AlbumDetailsDialog::~AlbumDetailsDialog()
{
    iCount--;
}

void AlbumDetailsDialog::show(AudioCdDevice *dev)
{
    udi=dev->udi();
    artist->setText(dev->albumArtist());
    title->setText(dev->albumName());
    genre->setText(dev->albumGenre());
    disc->setValue(dev->albumDisc());
    year->setValue(dev->albumYear());
    tracks->clear();
    QSet<QString> artists;
    artists.insert(dev->albumArtist());

    QList<Song> songs;
    foreach (const MusicLibraryItem *i, dev->childItems()) {
        songs.append(static_cast<const MusicLibraryItemSong *>(i)->song());
    }
    qSort(songs);

    foreach (const Song &s, songs) {
        QTreeWidgetItem *item=new QTreeWidgetItem(tracks);
        update(item, s);
        artists.insert(s.artist);
        item->setFlags(item->flags()|Qt::ItemIsEditable);
    }

    singleArtist->setChecked(1==artists.count());
    coverImage=dev->cover();
    if (!coverImage.img.isNull()) {
        cover->setPixmap(QPixmap::fromImage(coverImage.img.scaled(cover->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
    }
    Dialog::show();
}

void AlbumDetailsDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: {
        Device *dev=DevicesModel::self()->device(udi);
        if (dev && Device::AudioCd==dev->devType()) {
            CdAlbum cdAlbum;
            cdAlbum.artist=artist->text().trimmed();
            cdAlbum.name=title->text().trimmed();
            cdAlbum.disc=disc->value();
            cdAlbum.year=year->value();
            cdAlbum.genre=genre->text().trimmed();
            QString unknown=i18n("Unknown");
            if (cdAlbum.artist.isEmpty()) {
                cdAlbum.artist=unknown;
            }
            if (cdAlbum.name.isEmpty()) {
                cdAlbum.name=unknown;
            }
            for(int i=0; i<tracks->topLevelItemCount(); ++i) {
                QTreeWidgetItem *itm=tracks->topLevelItem(i);
                cdAlbum.tracks.append(toSong(itm));
            }
            static_cast<AudioCdDevice *>(dev)->setDetails(cdAlbum);
        }
        accept();
        break;
    }
    case Cancel:
        reject();
        break;
    default:
        break;
    }

    if (Ok==button) {
        accept();
    }

    Dialog::slotButtonClicked(button);
}

void AlbumDetailsDialog::hideArtistColumn(bool hide)
{
    tracks->header()->setSectionHidden(COL_ARTIST, hide);
}

void AlbumDetailsDialog::applyVa()
{
    if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Apply \"Various Artists\" workaround?")+
                                                        QLatin1String("<br/><hr/><br/>")+
                                                        i18n("<i>This will set 'Album artist' and 'Artist' to "
                                                             "\"Various Artists\", and set 'Title' to "
                                                             "\"TrackArtist - TrackTitle\"</i>"))) {
        return;
    }

    for(int i=0; i<tracks->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm=tracks->topLevelItem(i);
        Song s=toSong(itm);
        if (s.fixVariousArtists()) {
            update(itm, s);
        }
    }
}

void AlbumDetailsDialog::revertVa()
{
    if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Revert \"Various Artists\" workaround")+
                                                        QLatin1String("<br/><hr/><br/>")+
                                                        i18n("<i>Where the 'Album artist' is the same as 'Artist' "
                                                             "and the 'Title' is of the format \"TrackArtist - TrackTitle\", "
                                                             "'Artist' will be taken from 'Title' and 'Title' itself will be "
                                                             "set to just the title. e.g. <br/><br/>"
                                                             "If 'Title' is \"Wibble - Wobble\", then 'Artist' will be set to "
                                                             "\"Wibble\" and 'Title' will be set to \"Wobble\"</i>"))) {
        return;
    }

    for(int i=0; i<tracks->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm=tracks->topLevelItem(i);
        Song s=toSong(itm);
        if (s.revertVariousArtists()) {
            update(itm, s);
        }
    }
}

void AlbumDetailsDialog::capitalise()
{
    if (MessageBox::No==MessageBox::questionYesNo(this, i18n("Capitalize the first letter of 'Title', 'Artist', 'Album artist', and 'Album'"))) {
        return;
    }

    for(int i=0; i<tracks->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm=tracks->topLevelItem(i);
        Song s=toSong(itm);
        if (s.capitalise()) {
            update(itm, s);
        }
    }
}

void AlbumDetailsDialog::adjustTrackNumbers()
{
    bool ok=false;
    int adj=InputDialog::getInteger(i18n("Adjust Track Numbers"), i18n("Adjust track number by:"), 0, -500, 500, 1, 10, &ok, this);

    if (!ok || 0==adj) {
        return;
    }

    for(int i=0; i<tracks->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm=tracks->topLevelItem(i);
        Song s=toSong(itm);
        s.track+=adj;
        update(itm, s);
    }
}

Song AlbumDetailsDialog::toSong(QTreeWidgetItem *i)
{
    Song s;
    s.albumartist=artist->text().trimmed();
    s.album=title->text().trimmed();
    s.genre=genre->text().trimmed();
    s.year=year->value();
    s.disc=disc->value();
    s.artist=singleArtist->isChecked() ? s.albumartist : i->text(COL_ARTIST);
    s.title=i->text(COL_TITLE);
    s.track=i->text(COL_TRACK).toInt();
    s.id=i->data(0, Qt::UserRole).toInt();
    s.fillEmptyFields();
    return s;
}

void AlbumDetailsDialog::update(QTreeWidgetItem *i, const Song &s)
{
    i->setText(COL_TRACK, QString::number(s.track));
    i->setText(COL_ARTIST, s.artist);
    i->setText(COL_TITLE, s.title);
    i->setData(0, Qt::UserRole, s.id);
}

void AlbumDetailsDialog::setCover()
{
    int iconSize=cover->size().width()<=128 ? 128 : 256;
    cover->setPixmap(Icons::albumIcon.pixmap(iconSize, iconSize).scaled(cover->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
}

bool AlbumDetailsDialog::eventFilter(QObject *object, QEvent *event)
{
    switch(event->type()) {
    case QEvent::MouseButtonPress:
        if (Qt::LeftButton==static_cast<QMouseEvent *>(event)->button()) {
            pressed=true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (pressed && Qt::LeftButton==static_cast<QMouseEvent *>(event)->button()) {
            if (0==CoverDialog::instanceCount()) {
                CoverDialog *dlg=new CoverDialog(this);
                connect(dlg, SIGNAL(selectedCover(QImage, QString)), this, SLOT(coverSelected(QImage, QString)));
                Song s;
                s.file=AudioCdDevice::coverUrl(udi);
                s.artist=artist->text().trimmed();
                s.album=title->text().trimmed();
                dlg->show(s, coverImage);
            }
        }
        pressed=false;
        break;
    default:
        break;
    }
    return QObject::eventFilter(object, event);
}

void AlbumDetailsDialog::coverSelected(const QImage &img, const QString &fileName)
{
    coverImage=Covers::Image(img, fileName);
    if (!coverImage.img.isNull()) {
        cover->setPixmap(QPixmap::fromImage(coverImage.img.scaled(cover->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation)));
    }
}
