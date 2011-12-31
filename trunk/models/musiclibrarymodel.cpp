/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/*
 * Copyright (c) 2008 Sander Knopper (sander AT knopper DOT tk) and
 *                    Roeland Douma (roeland AT rullzer DOT com)
 *
 * This file is part of QtMPC.
 *
 * QtMPC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * QtMPC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with QtMPC.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "musiclibraryitemalbum.h"
#include "musiclibraryitemartist.h"
#include "musiclibraryitemsong.h"
#include "musiclibraryitemroot.h"
#include "musiclibrarymodel.h"
#include "playqueuemodel.h"
#include "settings.h"
#include "config.h"
#include "itemview.h"
#include "mpdparseutils.h"
#include "network.h"
#include <QCommonStyle>
#include <QFile>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>
#include <QDebug>
#include <QStringRef>
#include <QDateTime>
#include <QDir>
#include <QMimeData>
#include <QStringList>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KIcon>
#include <KDE/KLocale>
#endif

static const QLatin1String constLibraryCache("library/");
static const QLatin1String constLibraryExt(".xml");

MusicLibraryModel::MusicLibraryModel(QObject *parent)
    : QAbstractItemModel(parent),
      rootItem(new MusicLibraryItemRoot)
{
}

MusicLibraryModel::~MusicLibraryModel()
{
    delete rootItem;
}

QModelIndex MusicLibraryModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent))
        return QModelIndex();

    const MusicLibraryItem * parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<MusicLibraryItem *>(parent.internalPointer());

    MusicLibraryItem * const childItem = parentItem->child(row);
    if (childItem)
        return createIndex(row, column, childItem);

    return QModelIndex();
}

QModelIndex MusicLibraryModel::parent(const QModelIndex &index) const
{
    if (!index.isValid())
        return QModelIndex();

    const MusicLibraryItem * const childItem = static_cast<MusicLibraryItem *>(index.internalPointer());
    MusicLibraryItem * const parentItem = childItem->parent();

    if (parentItem == rootItem)
        return QModelIndex();

    return createIndex(parentItem->row(), 0, parentItem);
}

QVariant MusicLibraryModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    Q_UNUSED(section)
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data();

    return QVariant();
}

int MusicLibraryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() > 0)
        return 0;

    const MusicLibraryItem *parentItem;

    if (!parent.isValid())
        parentItem = rootItem;
    else
        parentItem = static_cast<MusicLibraryItem *>(parent.internalPointer());

    return parentItem->childCount();
}

int MusicLibraryModel::columnCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return static_cast<MusicLibraryItem *>(parent.internalPointer())->columnCount();
    else
        return rootItem->columnCount();
}

QVariant MusicLibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

    switch (role) {
    case Qt::DecorationRole:
        switch (item->type()) {
        case MusicLibraryItem::Type_Artist: {
            MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist *>(item);
            return artist->isVarious() ? QIcon::fromTheme("applications-multimedia") : QIcon::fromTheme("view-media-artist");
        }
        case MusicLibraryItem::Type_Album:
            if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
                return QIcon::fromTheme("media-optical-audio");
            } else {
                return static_cast<MusicLibraryItemAlbum *>(item)->cover();
            }
        case MusicLibraryItem::Type_Song:   return QIcon::fromTheme("audio-x-generic");
        default: return QVariant();
        }
    case Qt::DisplayRole:
        if (MusicLibraryItem::Type_Song==item->type()) {
            MusicLibraryItemSong *song = static_cast<MusicLibraryItemSong *>(item);
            if (song->track()>9) {
                return QString::number(song->track())+QLatin1String(" - ")+item->data();
            } else if (song->track()>0) {
                return QChar('0')+QString::number(song->track())+QLatin1String(" - ")+item->data();
            }
        } else if(MusicLibraryItem::Type_Album==item->type() && MusicLibraryItemAlbum::showDate() &&
                  static_cast<MusicLibraryItemAlbum *>(item)->year()>0) {
            return QString::number(static_cast<MusicLibraryItemAlbum *>(item)->year())+QLatin1String(" - ")+item->data();
        }
        return item->data();
    case Qt::ToolTipRole:
        switch (item->type()) {
        case MusicLibraryItem::Type_Artist:
            return 0==item->childCount()
                ? item->data()
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1\n1 Album", "%1\n%2 Albums", item->data(), item->childCount());
                    #else
                    (item->childCount()>1
                        ? tr("%1\n%2 Albums").arg(item->data()).arg(item->childCount())
                        : tr("%1\n1 Album").arg(item->data()));
                    #endif
        case MusicLibraryItem::Type_Album:
            return 0==item->childCount()
                ? item->data()
                :
                    #ifdef ENABLE_KDE_SUPPORT
                    i18np("%1\n1 Track", "%1\n%2 Tracks", item->data(), item->childCount());
                    #else
                    (item->childCount()>1
                        ? tr("%1\n%2 Tracks").arg(item->data()).arg(item->childCount())
                        : tr("%1\n1 Track").arg(item->data()));
                    #endif
        case MusicLibraryItem::Type_Song: {
            QString duration=MPDParseUtils::formatDuration(static_cast<MusicLibraryItemSong *>(item)->time());
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(3);
            }
            if (duration.startsWith(QLatin1String("00:"))) {
                duration=duration.mid(1);
            }
            return data(index, Qt::DisplayRole).toString()+QChar('\n')+duration;
        }
        default: return QVariant();
        }
    case ItemView::Role_ImageSize:
        if (MusicLibraryItem::Type_Album==item->type()) {
            return MusicLibraryItemAlbum::iconSize();
        }
        break;
    case ItemView::Role_SubText:
        switch (item->type()) {
        case MusicLibraryItem::Type_Artist:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Album", "%1 Albums", item->childCount());
            #else
            return 1==item->childCount() ? tr("1 Album") : tr("%1 Albums").arg(item->childCount());
            #endif
            break;
        case MusicLibraryItem::Type_Song: {
            QString text=MPDParseUtils::formatDuration(static_cast<MusicLibraryItemSong *>(item)->time());
            if (text.startsWith(QLatin1String("00:"))) {
                return text.mid(3);
            }
            if (text.startsWith(QLatin1String("00:"))) {
                return text.mid(1);
            }
            return text.mid(1);
        }
        case MusicLibraryItem::Type_Album:
            #ifdef ENABLE_KDE_SUPPORT
            return i18np("1 Track", "%1 Tracks", item->childCount());
            #else
            return 1==item->childCount() ? tr("1 Track") : tr("%1 Tracks").arg(item->childCount());
            #endif
        default: return QVariant();
        }
    case ItemView::Role_Image:
        if (MusicLibraryItem::Type_Album==item->type()) {
            QVariant v;
            v.setValue<QPixmap>(static_cast<MusicLibraryItemAlbum *>(item)->cover());
            return v;
        }
    default:
        return QVariant();
    }
    return QVariant();
}

void MusicLibraryModel::clear()
{
    const MusicLibraryItemRoot *oldRoot = rootItem;
    beginResetModel();
    databaseTime = QDateTime();
    rootItem = new MusicLibraryItemRoot;
    delete oldRoot;
    endResetModel();

    emit updated(rootItem);
    emit updateGenres(QStringList());
}

void MusicLibraryModel::updateMusicLibrary(MusicLibraryItemRoot *newroot, QDateTime db_update, bool fromFile)
{
//     libraryMutex.lock();
    const MusicLibraryItemRoot *oldRoot = rootItem;

    if (databaseTime > db_update) {
//         libraryMutex.unlock();
        return;
    }

    beginResetModel();

    databaseTime = db_update;
    rootItem = newroot;
    delete oldRoot;

    endResetModel();

    if (!fromFile) {
        toXML(db_update);
    }
//     libraryMutex.unlock();

    QStringList genres=QStringList(rootItem->genres().toList());
    genres.sort();
    emit updated(rootItem);
    emit updateGenres(genres);
}

void MusicLibraryModel::setCover(const QString &artist, const QString &album, const QImage &img)
{
    if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
        return;
    }

    if (img.isNull()) {
        return;
    }

    for (int i = 0; i < rootItem->childCount(); i++) {
        MusicLibraryItemArtist *artistItem = static_cast<MusicLibraryItemArtist*>(rootItem->child(i));
        if (artistItem->data()==artist) {
            for (int j = 0; j < artistItem->childCount(); j++) {
                MusicLibraryItemAlbum *albumItem = static_cast<MusicLibraryItemAlbum*>(artistItem->child(j));
                if (albumItem->data()==album) {
                    if (albumItem->setCover(img)) {
                        QModelIndex idx=index(j, 0, index(i, 0, QModelIndex()));
                        emit dataChanged(idx, idx);
                    }
                    break;
                }
            }
        }
    }
}

/**
 * Writes the musiclibrarymodel to and xml file so we can store it on
 * disk for faster startup the next time
 *
 * @param filename The name of the file to write the xml to
 */
void MusicLibraryModel::toXML(const QDateTime db_update)
{
    //Check if dir exists
    QString dir = Network::cacheDir(constLibraryCache);

    if (dir.isEmpty()) {
        qWarning("Couldn't create directory for storing database file");
        return;
    }

    //Create the filename
    QString hostname = Settings::self()->connectionHost();
    QString filename(QFile::encodeName(hostname + constLibraryExt));

    //Open the file
    QFile file(dir + filename);
    file.open(QIODevice::WriteOnly);

    //Write the header info
    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();

    //Start with the document
    writer.writeStartElement("MPD_database");
    writer.writeAttribute("version", "4");
    writer.writeAttribute("date", QString::number(db_update.toTime_t()));
    //Loop over all artist, albums and tracks.
    for (int i = 0; i < rootItem->childCount(); i++) {
        MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist*>(rootItem->child(i));
        writer.writeStartElement("Artist");
        writer.writeAttribute("name", artist->data());
        for (int j = 0; j < artist->childCount(); j++) {
            MusicLibraryItemAlbum *album = static_cast<MusicLibraryItemAlbum*>(artist->child(j));
            writer.writeStartElement("Album");
            writer.writeAttribute("title", album->data());
            writer.writeAttribute("dir", album->dir());
            writer.writeAttribute("year", QString::number(album->year()));
            for (int k = 0; k < album->childCount(); k++) {
                MusicLibraryItemSong *track = static_cast<MusicLibraryItemSong*>(album->child(k));
                writer.writeEmptyElement("Track");
                writer.writeAttribute("title", track->data());
                writer.writeAttribute("filename", track->file());
                writer.writeAttribute("time", QString::number(track->time()));
                //Only write track number if it is set
                if (track->track() != 0) {
                    writer.writeAttribute("track", QString::number(track->track()));
                }
                if (track->disc() != 0) {
                    writer.writeAttribute("disc", QString::number(track->disc()));
                }
                writer.writeAttribute("genre", track->genre());
            }
            writer.writeEndElement();
        }
        writer.writeEndElement();
    }

    writer.writeEndElement();
    writer.writeEndDocument();
    file.close();

    emit xmlWritten(db_update);
}

/**
 * Read an xml file from disk.
 *
 * @param filename The name of the xmlfile to read the db from
 *
 * @return true on succesfull parsing, false otherwise
 * TODO: check for hostname
 * TODO: check for database version
 */
bool MusicLibraryModel::fromXML(const QDateTime db_update)
{
    QString dir(Network::cacheDir(constLibraryCache, false));
    QString hostname = Settings::self()->connectionHost();
    QString filename(QFile::encodeName(hostname + constLibraryExt));

    //Check if file exists
    if (dir.isEmpty())
        return false;

    QFile file(dir + filename);

    MusicLibraryItemRoot * const rootItem = new MusicLibraryItemRoot;
    MusicLibraryItemArtist *artistItem = 0;
    MusicLibraryItemAlbum *albumItem = 0;
    MusicLibraryItemSong *songItem = 0;
    Song song;

    file.open(QIODevice::ReadOnly);

    QXmlStreamReader reader(&file);
    bool valid = false;

    while (!reader.atEnd()) {
        reader.readNext();

        /**
         * TODO: CHECK FOR ERRORS
         */
        if (reader.error()) {
            qDebug() << reader.errorString();
        } else {
            if (reader.isStartElement()) {
                QString element = reader.name().toString();

                if (element == "MPD_database") {
                    quint32 version = reader.attributes().value("version").toString().toUInt();
                    quint32 time_t = reader.attributes().value("date").toString().toUInt();

                    //Incompatible version
                    if (version < 4) {
                        break;
                    }

                    //Outdated
                    if (time_t != db_update.toTime_t())
                        break;

                    valid = true;
                }

                //Only check for other elements when we are valid!
                if (valid) {
                    //New artist element. Create it an add it
                    if (element == "Artist") {
                        song.artist=reader.attributes().value("name").toString();
                        artistItem = rootItem->artist(song);
                    }

                    // New album element. Create it and add it to the artist
                    if (element == "Album") {
                        song.album=reader.attributes().value("title").toString();
                        song.file=reader.attributes().value("dir").toString();
                        song.year=reader.attributes().value("year").toString().toUInt();
                        if (!song.file.isEmpty()) {
                            song.file.append("dummy.mp3");
                        }
                        albumItem = artistItem->album(song);
                    }

                    // New track element. Create it and add it to the album
                    if (element == "Track") {
                        song.title=reader.attributes().value("title").toString();
                        song.file=reader.attributes().value("filename").toString();

                        QString str=reader.attributes().value("track").toString();
                        song.track=str.isEmpty() ? 0 : str.toUInt();
                        str=reader.attributes().value("disc").toString();
                        song.disc=str.isEmpty() ? 0 : str.toUInt();
                        str=reader.attributes().value("time").toString();
                        song.time=str.isEmpty() ? 0 : str.toUInt();

                        songItem = new MusicLibraryItemSong(song, albumItem);

                        albumItem->append(songItem);

                        QString genre = reader.attributes().value("genre").toString();
                        albumItem->addGenre(genre);
                        artistItem->addGenre(genre);
                        songItem->addGenre(genre);
                        rootItem->addGenre(genre);
                    }
                }
            }
        }
    }

    //If not valid we need to cleanup
    if (!valid) {
        delete rootItem;
        return false;
    }

    file.close();
    updateMusicLibrary(rootItem, QDateTime(), true);
    return true;
}

Qt::ItemFlags MusicLibraryModel::flags(const QModelIndex &index) const
{
    if (index.isValid())
        return Qt::ItemIsSelectable | Qt::ItemIsDragEnabled | Qt::ItemIsEnabled;
    else
        return Qt::ItemIsDropEnabled;
}

/**
* Convert the data at indexes into mimedata ready for transport
*
* @param indexes The indexes to pack into mimedata
* @return The mimedata
*/
QMimeData *MusicLibraryModel::mimeData(const QModelIndexList &indexes) const
{
    QMimeData *mimeData = new QMimeData();
    QStringList filenames;

    foreach(QModelIndex index, indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

        switch (item->type()) {
        case MusicLibraryItem::Type_Artist:
            for (int i = 0; i < item->childCount(); i++) {
                filenames << static_cast<MusicLibraryItemAlbum*>(item->child(i))->sortedTracks();
            }
            break;
        case MusicLibraryItem::Type_Album:
            filenames << static_cast<MusicLibraryItemAlbum*>(item)->sortedTracks();
            break;
        case MusicLibraryItem::Type_Song:
            if (item->type() == MusicLibraryItem::Type_Song && !filenames.contains(static_cast<MusicLibraryItemSong*>(item)->file())) {
                filenames << static_cast<MusicLibraryItemSong*>(item)->file();
            }
            break;
        default:
            break;
        }
    }

    PlayQueueModel::encode(*mimeData, PlayQueueModel::constFileNameMimeType, filenames);
    return mimeData;
}
