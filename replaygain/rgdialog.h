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

#ifndef _RGDIALOG_H_
#define _RGDIALOG_H_

#include <KDE/KDialog>
#include "scanner.h"
#include "song.h"

class QTreeWidget;
class QProgressBar;
class Device;

class RgDialog : public KDialog
{
    Q_OBJECT

public:
    RgDialog(QWidget *parent);
    void show(const QList<Song> &songs, const QString &basePath);

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void update();

private:
    void slotButtonClicked(int button);
    void startScanning();
    void stopScanning();
    void saveTags();
    void updateView();
    Device * getDevice(const QString &udi, QWidget *p);

private Q_SLOTS:
    void scannerProgress(Scanner *s, int p);
    void scannerDone(ThreadWeaver::Job *j);

private:
    QTreeWidget *view;
    QProgressBar *progress;
    bool scanning;
    QString base;
    QList<Song> origSongs;
    struct JobStatus
    {
        JobStatus(int i) : index(i), progress(0), finished(false) { }
        int index;
        int progress;
        bool finished;
    };
    QMap<Scanner *, JobStatus> jobs;
    QMap<int, Scanner *> scanners;
    struct Album
    {
        QList<int> tracks;
        Scanner::Data data;
    };
    QMap<QString, Album> albums;
};

#endif
