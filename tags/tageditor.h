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
#include "widgets/songdialog.h"
#include "ui_tageditor.h"
#include <QSet>
#include <QList>

#ifdef ENABLE_DEVICES_SUPPORT
class Device;
#endif

class TagEditor : public SongDialog, Ui::TagEditor
{
    Q_OBJECT

public:
    static int instanceCount();

    TagEditor(QWidget *parent, const QList<Song> &songs,
              const QSet<QString> &existingArtists, const QSet<QString> &existingAlbumArtists, const QSet<QString> &existingComposers,
              const QSet<QString> &existingAlbums, const QSet<QString> &existingGenres, const QString &udi);
    ~TagEditor() override;

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void update();
    void getRating(const QString &f);
    void setRating(const QString &f, quint8 r);

private:
    void enableOkButton();
    void setLabelStates();
    void setVariousHint();
    void fillSong(Song &s, bool isAll, bool skipEmpty) const;
    void slotButtonClicked(int button) override;
    void updateTrackName(int index, bool edited);
    void updateEditedStatus(int index);
    bool applyUpdates();
    #ifdef ENABLE_DEVICES_SUPPORT
    Device * getDevice(const QString &udi, QWidget *p);
    #endif
    void closeEvent(QCloseEvent *event) override;
    void controlInitialActionsState();

private Q_SLOTS:
    void readComments();
    void applyVa();
    void revertVa();
    void setAlbumArtistFromArtist();
    void capitalise();
    void checkChanged();
    void adjustTrackNumbers();
    void readRatings();
    void writeRatings();
    void updateEdited(bool isFromAll=false);
    void setSong(const Song &s);
    void setIndex(int idx);
    void rating(const QString &f, quint8 r);
    void checkRating();

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
    bool haveArtists;
    bool haveAlbumArtists;
    bool haveComposers;
    bool haveComments;
    bool haveAlbums;
    bool haveGenres;
    bool haveDiscs;
    bool haveYears;
    bool haveRatings;
    bool saving;
    bool composerSupport;
    bool commentSupport;
    QAction *readRatingsAct;
    QAction *writeRatingsAct;
};

#endif
