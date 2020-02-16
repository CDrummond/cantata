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
 * General Public License for more detailexampleSong.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "albumdetailsdialog.h"
#include "audiocddevice.h"
#include "models/musiclibraryitemsong.h"
#include "support/messagebox.h"
#include "support/inputdialog.h"
#include "models/devicesmodel.h"
#include "models/mpdlibrarymodel.h"
#include "cdalbum.h"
#include "widgets/icons.h"
#include "gui/coverdialog.h"
#include "widgets/basicitemdelegate.h"
#include "support/lineedit.h"
#include <QMenu>
#include <QStyledItemDelegate>
#include <QMouseEvent>
#include <QSpinBox>
#include <algorithm>

enum Columns {
    COL_TRACK,
    COL_ARTIST,
    COL_TITLE
};

class EditorDelegate : public BasicItemDelegate
{
public:
    EditorDelegate(QObject *parent=0) : BasicItemDelegate(parent) { }

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        Q_UNUSED(option)
        if (COL_TRACK==index.column()) {
            QSpinBox *editor = new QSpinBox(parent);
            editor->setMinimum(0);
            editor->setMaximum(500);
            return editor;
        } else {
            parent->setProperty("cantata-delegate", true);
            return new LineEdit(parent);
        }
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const {
        return QStyledItemDelegate::sizeHint(option, index)+QSize(0, 4);
    }

    void setEditorData(QWidget *editor, const QModelIndex &index) const {
        if (COL_TRACK==index.column()) {
            static_cast<QSpinBox*>(editor)->setValue(index.model()->data(index, Qt::EditRole).toInt());
        } else {
            static_cast<LineEdit*>(editor)->setText(index.model()->data(index, Qt::EditRole).toString());
        }
    }
    void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const {
        if (COL_TRACK==index.column()) {
            model->setData(index, static_cast<QSpinBox*>(editor)->value(), Qt::EditRole);
        } else {
            model->setData(index, static_cast<LineEdit*>(editor)->text().trimmed(), Qt::EditRole);
        }
    }

    void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const {
        Q_UNUSED(index)
        editor->setGeometry(option.rect);
    }
};

static int iCount=0;

int AlbumDetailsDialog::instanceCount()
{
    return iCount;
}

AlbumDetailsDialog::AlbumDetailsDialog(QWidget *parent)
    : Dialog(parent, "AlbumDetailsDialog")
    , pressed(false)
{
    iCount++;
    setButtons(User1|Ok|Cancel);
    setCaption(tr("Audio CD"));
    setAttribute(Qt::WA_DeleteOnClose);

    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    setMainWidget(mainWidet);

    QSet<QString> artists;
    QSet<QString> albumArtists;
    QSet<QString> composers;
    QSet<QString> albums;
    QSet<QString> genres;
    MpdLibraryModel::self()->getDetails(artists, albumArtists, composers, albums, genres);

    QStringList strings=albumArtists.toList();
    strings.sort();
    artist->clear();
    artist->insertItems(0, strings);

    strings=composers.toList();
    strings.sort();
    composer->clear();
    composer->insertItems(0, strings);

    strings=albums.toList();
    strings.sort();
    title->clear();
    title->insertItems(0, strings);

    strings=genres.toList();
    strings.sort();
    genre->clear();
    genre->insertItems(0, strings);

    QMenu *toolsMenu=new QMenu(this);
    toolsMenu->addAction(tr("Apply \"Various Artists\" Workaround"), this, SLOT(applyVa()));
    toolsMenu->addAction(tr("Revert \"Various Artists\" Workaround"), this, SLOT(revertVa()));
    toolsMenu->addAction(tr("Capitalize"), this, SLOT(capitalise()));
    toolsMenu->addAction(tr("Adjust Track Numbers"), this, SLOT(adjustTrackNumbers()));
    setButtonMenu(User1, toolsMenu, InstantPopup);
    setButtonGuiItem(User1, GuiItem(tr("Tools"), "tools-wizard"));
    connect(singleArtist, SIGNAL(toggled(bool)), SLOT(hideArtistColumn(bool)));
    resize(600, 600);

    int size=fontMetrics().height()*5;
    cover->setMinimumSize(size, size);
    cover->setMaximumSize(size, size);
    setCover();
    cover->installEventFilter(this);
    tracks->setItemDelegate(new EditorDelegate(tracks));
}

AlbumDetailsDialog::~AlbumDetailsDialog()
{
    iCount--;
}

void AlbumDetailsDialog::show(AudioCdDevice *dev)
{
    udi=dev->id();
    artist->setText(dev->albumArtist());
    composer->setText(dev->albumComposer());
    title->setText(dev->albumName());
    genre->setText(dev->albumGenre());
    disc->setValue(dev->albumDisc());
    year->setValue(dev->albumYear());
    tracks->clear();
    QSet<QString> artists;
    artists.insert(dev->albumArtist());

    QList<Song> songs;
    for (const MusicLibraryItem *i: dev->childItems()) {
        songs.append(static_cast<const MusicLibraryItemSong *>(i)->song());
    }
    std::sort(songs.begin(), songs.end());

    for (const Song &s: songs) {
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
            CdAlbum cdAlbum=getAlbum();
            for(int i=0; i<tracks->topLevelItemCount(); ++i) {
                cdAlbum.tracks.append(toSong(tracks->topLevelItem(i), cdAlbum));
            }
            static_cast<AudioCdDevice *>(dev)->setDetails(cdAlbum);
            static_cast<AudioCdDevice *>(dev)->setCover(coverImage);
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
    if (MessageBox::No==MessageBox::questionYesNo(this, tr("Apply \"Various Artists\" workaround?")+
                                                        QLatin1String("\n\n")+
                                                        tr("This will set 'Album artist' and 'Artist' to "
                                                             "\"Various Artists\", and set 'Title' to "
                                                             "\"TrackArtist - TrackTitle\""), tr("Apply \"Various Artists\" Workaround"),
                                                  StdGuiItem::apply(), StdGuiItem::cancel())) {
        return;
    }

    CdAlbum album=getAlbum();
    for(int i=0; i<tracks->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm=tracks->topLevelItem(i);
        Song s=toSong(itm, album);
        if (s.fixVariousArtists()) {
            update(itm, s);
        }
    }
}

void AlbumDetailsDialog::revertVa()
{
    if (MessageBox::No==MessageBox::questionYesNo(this, tr("Revert \"Various Artists\" workaround?")+
                                                        QLatin1String("\n\n")+
                                                        tr("Where the 'Album artist' is the same as 'Artist' "
                                                             "and the 'Title' is of the format \"TrackArtist - TrackTitle\", "
                                                             "'Artist' will be taken from 'Title' and 'Title' itself will be "
                                                             "set to just the title. e.g. \n"
                                                             "If 'Title' is \"Wibble - Wobble\", then 'Artist' will be set to "
                                                             "\"Wibble\" and 'Title' will be set to \"Wobble\""), tr("Revert \"Various Artists\" Workaround"),
                                                  GuiItem(tr("Revert")), StdGuiItem::cancel())) {
        return;
    }

    CdAlbum album=getAlbum();
    for(int i=0; i<tracks->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm=tracks->topLevelItem(i);
        Song s=toSong(itm, album);
        if (s.revertVariousArtists()) {
            update(itm, s);
        }
    }
}

void AlbumDetailsDialog::capitalise()
{
    if (MessageBox::No==MessageBox::questionYesNo(this, tr("Capitalize the first letter of 'Title', 'Artist', 'Album artist', and 'Album'?"),
                                                  tr("Capitalize"), GuiItem(tr("Capitalize")), StdGuiItem::cancel())) {
        return;
    }

    CdAlbum album=getAlbum();
    for(int i=0; i<tracks->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm=tracks->topLevelItem(i);
        Song s=toSong(itm, album);
        if (s.capitalise()) {
            update(itm, s);
        }
    }
}

void AlbumDetailsDialog::adjustTrackNumbers()
{
    bool ok=false;
    int adj=InputDialog::getInteger(tr("Adjust Track Numbers"), tr("Adjust track number by:"), 0, -500, 500, 1, 10, &ok, this);

    if (!ok || 0==adj) {
        return;
    }

    CdAlbum album=getAlbum();
    for(int i=0; i<tracks->topLevelItemCount(); ++i) {
        QTreeWidgetItem *itm=tracks->topLevelItem(i);
        Song s=toSong(itm, album);
        s.track+=adj;
        update(itm, s);
    }
}

enum Roles {
    Role_Id = Qt::UserRole,
    Role_File,
    Role_Time
};

Song AlbumDetailsDialog::toSong(QTreeWidgetItem *i, const CdAlbum &album)
{
    Song s;
    s.albumartist=album.artist;
    s.album=album.name;
    s.genres[0]=album.genre;
    s.year=album.year;
    s.disc=album.disc;
    s.artist=singleArtist->isChecked() ? s.albumartist : i->text(COL_ARTIST);
    if (!album.composer.isEmpty()) {
        s.setComposer(album.composer);
    }
    s.title=i->text(COL_TITLE);
    s.track=i->text(COL_TRACK).toInt();
    s.id=i->data(0, Role_Id).toInt();
    s.file=i->data(0, Role_File).toString();
    s.time=i->data(0, Role_Time).toInt();
    s.fillEmptyFields();
    return s;
}

CdAlbum AlbumDetailsDialog::getAlbum() const
{
    CdAlbum cdAlbum;
    cdAlbum.artist=artist->text().trimmed();
    cdAlbum.composer=composer->text().trimmed();
    cdAlbum.name=title->text().trimmed();
    cdAlbum.disc=disc->value();
    cdAlbum.year=year->value();
    cdAlbum.genre=genre->text().trimmed();
    if (cdAlbum.artist.isEmpty()) {
        cdAlbum.artist=Song::unknown();
    }
    if (cdAlbum.name.isEmpty()) {
        cdAlbum.name=Song::unknown();
    }
    return cdAlbum;
}

void AlbumDetailsDialog::update(QTreeWidgetItem *i, const Song &s)
{
    i->setText(COL_TRACK, QString::number(s.track));
    i->setText(COL_ARTIST, s.artist);
    i->setText(COL_TITLE, s.title);
    i->setData(0, Role_Id, s.id);
    i->setData(0, Role_File, s.file);
    i->setData(0, Role_Time, s.time);
}

void AlbumDetailsDialog::setCover()
{
    int iconSize=cover->size().width()<=128 ? 128 : 256;
    cover->setPixmap(Icons::self()->albumIcon(iconSize).pixmap(iconSize, iconSize).scaled(cover->size(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
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
                s.type=Song::Cdda;
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

#include "moc_albumdetailsdialog.cpp"
