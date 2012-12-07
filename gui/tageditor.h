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

#ifndef TAG_EDITOR_H
#define TAG_EDITOR_H

#include "config.h"
#include "dialog.h"
#include "ui_tageditor.h"
#include "song.h"
#include <QtCore/QSet>
#include <QtCore/QList>

#ifdef ENABLE_DEVICES_SUPPORT
class Device;
#endif

class TagEditor : public Dialog, Ui::TagEditor
{
    Q_OBJECT

public:
    static int instanceCount();

    TagEditor(QWidget *parent, const QList<Song> &songs,
              const QSet<QString> &existingArtists, const QSet<QString> &existingAlbumArtists,
              const QSet<QString> &existingAlbums, const QSet<QString> &existingGenres
              #ifdef ENABLE_DEVICES_SUPPORT
              , const QString &udi
              #endif
             );
    virtual ~TagEditor();

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void update();

private:
    void enableOkButton();
    void setLabelStates();
    void setPlaceholderTexts();
    void fillSong(Song &s, bool isAll, bool skipEmpty) const;
    void slotButtonClicked(int button);
    void updateTrackName(int index, bool edited);
    void updateEditedStatus(int index);
    void applyUpdates();
    #ifdef ENABLE_DEVICES_SUPPORT
    Device * getDevice(const QString &udi, QWidget *p);
    #endif
    void closeEvent(QCloseEvent *event);

private Q_SLOTS:
    void applyVa();
    void revertVa();
    void setAlbumArtistFromArtist();
    void capitalise();
    void checkChanged();
    void adjustTrackNumbers();
    void updateEdited(bool isFromAll=false);
    void setSong(const Song &s);
    void setIndex(int idx);

private:
    QString baseDir;
    #ifdef ENABLE_DEVICES_SUPPORT
    QString deviceUdi;
    #endif
    QList<Song> original;
    QList<Song> edited;
    int currentSongIndex;
    QSet<int> editedIndexes;
    bool updating;
    bool saveable;
    bool haveArtists;
    bool haveAlbumArtists;
    bool haveAlbums;
    bool haveGenres;
    bool saving;
};

#endif
