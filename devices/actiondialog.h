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

#ifndef ACTION_DIALOG_H
#define ACTION_DIALOG_H

#include "support/dialog.h"
#include "mpd-interface/song.h"
#include "device.h"
#include "config.h"
#include "ui_actiondialog.h"
#include <QElapsedTimer>
#ifdef QT_QTDBUS_FOUND
#include <QDBusMessage>
#endif
#include <QList>
#include <QPair>

class SongListDialog;

class ActionDialog : public Dialog, Ui::ActionDialog
{
    Q_OBJECT

    enum Mode
    {
        Copy,
        Remove,
        Sync
    };

    typedef QPair<QString, QString> StringPair;
    typedef QList<StringPair> StringPairList;

public:
    static int instanceCount();

    ActionDialog(QWidget *parent);
    ~ActionDialog() override;

    void sync(const QString &devId, const QList<Song> &libSongs, const QList<Song> &devSongs);
    void copy(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs);
    void remove(const QString &udi, const QList<Song> &songs);

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void update();

    void completed();

private Q_SLOTS:
    void calcFileSize();
    void configureSource();
    void configureDest();
    void saveProperties(const QString &path, const DeviceOptions &opts);
    void saveProperties();
    void actionStatus(int status, bool copiedCover=false);
    void doNext();
    void removeSongResult(int status);
    void cleanDirsResult(int status);
    void jobPercent(int percent);
    void cacheSaved();
    void controlInfoLabel();
    void deviceRenamed();

private:
    void updateSongCountLabel();
    void controlInfoLabel(Device *dev);
    Device * getDevice(const QString &udi, bool logErrors=true);
    void configure(const QString &udi);
    void init(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs, Mode m);
    void slotButtonClicked(int button) override;
    void setPage(int page, const StringPairList &msg=StringPairList(), const QString &header=QString());
    StringPairList formatSong(const Song &s, bool showFiles=false);
    bool refreshLibrary();
    void removeSong(const Song &s);
    void cleanDirs();
    void incProgress();
    void updateUnity(bool finished);

private:
    Mode mode;
    qint64 spaceRequired;
    bool sourceIsAudioCd;
    QString sourceUdi;
    QString destUdi;
    QList<Song> songsToCalcSize;
    QList<Song> songsToAction;
    QList<Song> skippedSongs;
    QList<Song> actionedSongs;
    QList<Song> syncSongs;
    QSet<QString> dirsToClean;
    QSet<QString> copiedCovers;
    unsigned long count;
    int currentPercent; // Percentage of current song
    Song origCurrentSong;
    Song currentSong;
    bool autoSkip;
    bool paused;
    bool performingAction;
    bool haveVariousArtists;
    bool mpdConfigured;
    Device *currentDev;
    QString destFile;
    DeviceOptions namingOptions;
    #ifdef ENABLE_REPLAYGAIN_SUPPORT
    QSet<QString> albumsWithoutRgTags;
    #endif
    #ifdef QT_QTDBUS_FOUND
    QDBusMessage unityMessage;
    #endif

    SongListDialog *songDialog;
    friend class SongListDialog;
};

#endif
