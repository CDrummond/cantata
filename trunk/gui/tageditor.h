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
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KDialog>
#else
#include <QtGui/QDialog>
class QDialogButtonBox;
class QAbstractButton;
#endif
#include "ui_tageditor.h"
#include "song.h"
#include <QtCore/QSet>
#include <QtCore/QList>

#ifdef ENABLE_DEVICES_SUPPORT
class Device;
#endif

#ifdef ENABLE_KDE_SUPPORT
class TagEditor : public KDialog, Ui::TagEditor
#else
class TagEditor : public QDialog, Ui::TagEditor
#endif
{
    Q_OBJECT

public:
    TagEditor(QWidget *parent, const QList<Song> &songs,
              const QSet<QString> &existingArtists, const QSet<QString> &existingAlbumArtists,
              const QSet<QString> &existingAlbums, const QSet<QString> &existingGenres
              #ifdef ENABLE_DEVICES_SUPPORT
              , const QString &udi
              #endif
             );
    virtual ~TagEditor() {
    }

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void update();

private:
    void enableOkButton();
    void setLabelStates();
    void fillSong(Song &s, bool isAll, bool skipEmpty) const;
    #ifdef ENABLE_KDE_SUPPORT
    void slotButtonClicked(int button);
    #endif
    void updateTrackName(int index, bool edited);
    void updateEditedStatus(int index);
    void applyUpdates();
    #ifdef ENABLE_DEVICES_SUPPORT
    Device * getDevice(const QString &udi, QWidget *p);
    #endif

private Q_SLOTS:
    void applyVa();
    void revertVa();
    void capitalise();
    void checkChanged();
    void updateEdited(bool isFromAll=false);
    void setSong(const Song &s);
    void setIndex(int idx);
    #ifndef ENABLE_KDE_SUPPORT
    void buttonPressed(QAbstractButton *button);
    #endif

private:
    #ifndef ENABLE_KDE_SUPPORT
    QDialogButtonBox *buttonBox;
    #endif
    QString baseDir;
    #ifdef ENABLE_DEVICES_SUPPORT
    bool isDevice;
    #endif
    QList<Song> original;
    QList<Song> edited;
    int currentSongIndex;
    QSet<int> editedIndexes;
    bool updating;
    bool saveable;
};

#endif
