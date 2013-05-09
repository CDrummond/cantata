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

#ifndef _RGDIALOG_H_
#define _RGDIALOG_H_

#include "dialog.h"
#include "scanner.h"
#include "song.h"
#include "tags.h"
#include "config.h"

class QTreeWidget;
class QLabel;
class QProgressBar;
#ifdef ENABLE_DEVICES_SUPPORT
class Device;
#endif
class TagReader;

class RgDialog : public Dialog
{
    Q_OBJECT

public:
    static int instanceCount();

    RgDialog(QWidget *parent);
    virtual ~RgDialog();

    void show(const QList<Song> &songs, const QString &udi);

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void update();

private:
    void slotButtonClicked(int button);
    void startScanning();
    void stopScanning();
    void createScanner(int index);
    void clearScanners();
    void startReadingTags();
    void stopReadingTags();
    void saveTags();
    void updateView();
    #ifdef ENABLE_DEVICES_SUPPORT
    Device * getDevice(const QString &udi, QWidget *p);
    #endif
    void closeEvent(QCloseEvent *event);

private Q_SLOTS:
    void scannerProgress(int p);
    void scannerDone();
    void songTags(int index, Tags::ReplayGain tags);
    void tagReaderDone();

private:
    enum State {
        State_Idle,
        State_ScanningTags,
        State_ScanningFiles,
        State_Saving
    };

    struct Album {
        QList<int> tracks;
        QSet<int> scannedTracks;
        Scanner::Data data;
    };

    struct Track {
        Track() : progress(0), finished(false), success(false) { }
        unsigned char progress;
        bool finished : 1;
        bool success : 1;
        Scanner::Data data;
    };

    QTreeWidget *view;
    QLabel *statusLabel;
    QProgressBar *progress;
    State state;
    QString base;
    QList<Song> origSongs;

    QMap<int, Scanner *> scanners;
    QList<int> toScan;
    int totalToScan;

    QMap<QString, Album> albums;
    QMap<int, Track> tracks;

    QMap<int, Tags::ReplayGain> origTags;
    QSet<int> needToSave;
    TagReader *tagReader;
};

#endif
