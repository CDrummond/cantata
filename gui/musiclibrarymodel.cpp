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

#include "gui/musiclibraryitemalbum.h"
#include "gui/musiclibraryitemartist.h"
#include "gui/musiclibraryitemsong.h"
#include "gui/musiclibraryitemroot.h"
#include "musiclibrarymodel.h"
#include "settings.h"
#include "config.h"

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
#endif

QString MusicLibraryModel::cacheDir(const QString &sub, bool create)
{
    QString env = qgetenv("XDG_CACHE_HOME");
    QString dir = (env.isEmpty() ? QDir::homePath() + "/.cache/" : env) + PACKAGE_NAME"/";
    if(!sub.isEmpty()) {
        dir+=sub+'/';
    }
    QDir d(dir);
    return d.exists() || (create && d.mkpath(dir)) ? QDir::toNativeSeparators(dir) : QString();
}

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
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return rootItem->data(section);

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
        case MusicLibraryItem::Type_Artist: return QIcon::fromTheme("view-media-artist");
        case MusicLibraryItem::Type_Album:
            if (MusicLibraryItemAlbum::CoverNone==MusicLibraryItemAlbum::currentCoverSize()) {
                return QIcon::fromTheme("media-optical-audio");
            } else {
                return static_cast<MusicLibraryItemAlbum *>(item)->cover();
            }
        // Any point to a track icon?
        //case MusicLibraryItem::Type_Song:   return QIcon::fromTheme("audio-x-generic");
        default: return QVariant();
        }
    case Qt::DisplayRole:
    case Qt::ToolTipRole:
        return item->data(index.column());
    default:
        return QVariant();
    }
}

void MusicLibraryModel::updateMusicLibrary(MusicLibraryItemRoot *newroot, QDateTime db_update, bool fromFile)
{
    libraryMutex.lock();
    const MusicLibraryItemRoot *oldRoot = rootItem;

    if (databaseTime > db_update) {
        libraryMutex.unlock();
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
    libraryMutex.unlock();

    QStringList genres=QStringList(rootItem->genres().toList());
    genres.sort();
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
        if (artistItem->data(0).toString()==artist) {
            for (int j = 0; j < artistItem->childCount(); j++) {
                MusicLibraryItemAlbum *albumItem = static_cast<MusicLibraryItemAlbum*>(artistItem->child(j));
                if (albumItem->data(0).toString()==album) {
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
    QString dir = cacheDir(constLibraryCache);

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
    writer.writeAttribute("version", "1");
    writer.writeAttribute("date", QString::number(db_update.toTime_t()));
    //Loop over all artist, albums and tracks.
    for (int i = 0; i < rootItem->childCount(); i++) {
        MusicLibraryItemArtist *artist = static_cast<MusicLibraryItemArtist*>(rootItem->child(i));
        writer.writeStartElement("Artist");
        writer.writeAttribute("name", artist->data(0).toString());
        for (int j = 0; j < artist->childCount(); j++) {
            MusicLibraryItemAlbum *album = static_cast<MusicLibraryItemAlbum*>(artist->child(j));
            writer.writeStartElement("Album");
            writer.writeAttribute("title", album->data(0).toString());
            for (int k = 0; k < album->childCount(); k++) {
                MusicLibraryItemSong *track = static_cast<MusicLibraryItemSong*>(album->child(k));
                writer.writeEmptyElement("Track");
                writer.writeAttribute("title", track->data(0).toString());
                writer.writeAttribute("filename", track->file());
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
    QString dir(cacheDir(constLibraryCache, false));
    QString hostname = Settings::self()->connectionHost();
    QString filename(QFile::encodeName(hostname + constLibraryExt));

    //Check if file exists
    if (dir.isEmpty())
        return false;

    QFile file(dir + filename);

    MusicLibraryItemRoot * const rootItem = new MusicLibraryItemRoot("Artist / Album / Song");
    MusicLibraryItemArtist *artistItem = NULL;
    MusicLibraryItemAlbum *albumItem = NULL;
    MusicLibraryItemSong *songItem = NULL;

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
                    if (version < 1) {
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
                        QString artist_string = reader.attributes().value("name").toString();

                        artistItem = new MusicLibraryItemArtist(artist_string, rootItem);
                        rootItem->appendChild(artistItem);
                    }

                    // New album element. Create it and add it to the artist
                    if (element == "Album") {
                        QString album_string = reader.attributes().value("title").toString();

                        albumItem = new MusicLibraryItemAlbum(album_string, artistItem);
                        artistItem->appendChild(albumItem);
                    }

                    // New track element. Create it and add it to the album
                    if (element == "Track") {
                        QString track_name = reader.attributes().value("title").toString();
                        songItem = new MusicLibraryItemSong(track_name, albumItem);

                        QString track_file = reader.attributes().value("filename").toString();
                        songItem->setFile(track_file);

                        QString track_number_string = reader.attributes().value("track").toString();
                        QString disc_number_string = reader.attributes().value("disc").toString();
                        QString genre = reader.attributes().value("genre").toString();
                        if (!track_number_string.isEmpty()) {
                            quint32 track_number = track_number_string.toUInt();
                            quint32 disc_number = disc_number_string.toUInt();
                            songItem->setTrack(track_number);
                            songItem->setDisc(disc_number);
                        }

                        albumItem->appendChild(songItem);
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
    QByteArray encodedData;

    QDataStream stream(&encodedData, QIODevice::WriteOnly);
    QStringList filenames;

    foreach(QModelIndex index, indexes) {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());

        switch (item->type()) {
        case MusicLibraryItem::Type_Artist:
            for (int i = 0; i < item->childCount(); i++) {
                filenames << sortAlbumTracks(static_cast<MusicLibraryItemAlbum*>(item->child(i)));
            }
            break;
        case MusicLibraryItem::Type_Album:
            filenames << sortAlbumTracks(static_cast<MusicLibraryItemAlbum*>(item));
            break;
        case MusicLibraryItem::Type_Song:
            if (item->type() == MusicLibraryItem::Type_Song) {
                if (!filenames.contains(static_cast<MusicLibraryItemSong*>(item)->file()))
                    filenames << static_cast<MusicLibraryItemSong*>(item)->file();
            }
            break;
        default:
            break;
        }
    }

    for (int i = filenames.size() - 1; i >= 0; i--) {
        stream << filenames.at(i);
    }

    mimeData->setData("application/cantata_songs_filename_text", encodedData);
    return mimeData;
}

/**
 * Sort an album by its track numbers. All unnumberd tracks are added to the end
 *
 * @param album The album musiclibrary item
 */
QStringList MusicLibraryModel::sortAlbumTracks(const MusicLibraryItemAlbum *album) const
{
    if (album->type() != MusicLibraryItem::Type_Album) {
        return QStringList();
    }

    MusicLibraryItemAlbum *orderedTracks = new MusicLibraryItemAlbum("Album");
    QStringList unorderedTracks;

    for (int i = 0; i < album->childCount(); i++) {
        MusicLibraryItemSong *tmp_track = static_cast<MusicLibraryItemSong*>(album->child(i));
        MusicLibraryItemSong *track = new MusicLibraryItemSong("Song");
        track->setTrack(tmp_track->track());
        track->setDisc(tmp_track->disc());
        track->setFile(tmp_track->file());

        if (track->track() == (quint32)0) {
            unorderedTracks << track->file();
        }

        bool insert = false;
        for (int j = 0; j < orderedTracks->childCount(); j++) {
            if (track->track() < static_cast<MusicLibraryItemSong*>(orderedTracks->child(j))->track()) {
                orderedTracks->insertChild(track, j);
                insert = true;
                break;
            }
        }

        if (insert == false) {
            orderedTracks->insertChild(track, orderedTracks->childCount());
        }
    }

    QStringList tracks;
    for (int i = 0; i < orderedTracks->childCount(); i++) {
        tracks << static_cast<MusicLibraryItemSong*>(orderedTracks->child(i))->file();
    }
    tracks << unorderedTracks;

    orderedTracks->clearChildren();
    delete orderedTracks;
    return tracks;
}
