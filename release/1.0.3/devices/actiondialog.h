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

#ifndef ACTION_DIALOG_H
#define ACTION_DIALOG_H

#include "dialog.h"
#include "song.h"
#include "device.h"
#include "ui_actiondialog.h"
#include <QElapsedTimer>

class ActionDialog : public Dialog, Ui::ActionDialog
{
    Q_OBJECT

    enum Mode
    {
        Copy,
        Remove
    };

public:
    static int instanceCount();

    ActionDialog(QWidget *parent);
    virtual ~ActionDialog();

    void copy(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs);
    void remove(const QString &udi, const QList<Song> &songs);

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void update();

    void completed();

private Q_SLOTS:
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

private:
    void controlInfoLabel(Device *dev);
    Device * getDevice(const QString &udi, bool logErrors=true);
    void configure(const QString &udi);
    void init(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs, Mode m);
    void slotButtonClicked(int button);
    void setPage(int page, const QString &msg=QString());
    QString formatSong(const Song &s, bool showFiles=false, bool showTime=false);
    bool refreshLibrary();
    void removeSong(const Song &s);
    void cleanDirs();
    void incProgress();

private:
    Mode mode;
    QString sourceUdi;
    QString destUdi;
    QList<Song> songsToAction;
    QList<Song> skippedSongs;
    QList<Song> actionedSongs;
    QSet<QString> dirsToClean;
    QSet<QString> copiedCovers;
    unsigned long count;
    #ifdef ACTION_DIALOG_SHOW_TIME_REMAINING
    double totalTime; // Time of all songs
    double actionedTime; // Time of songs that have currently been actioned
    quint64 timeTaken; // Amount of time spent copying/deleting
    QElapsedTimer timer;
    #endif
    double currentPercent; // Percentage of current song
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
};

#endif
