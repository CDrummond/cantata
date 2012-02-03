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

#ifndef TRACKORGANISER_H
#define TRACKORGANISER_H

#include <KDE/KDialog>
#include "ui_trackorganiser.h"
#include "song.h"
#include "device.h"

class FilenameSchemeDialog;
class TrackOrganiser : public KDialog, Ui::TrackOrganiser
{
    Q_OBJECT

public:
    TrackOrganiser(QWidget *parent);
    void show(const QList<Song> &songs, const QString &udi);

Q_SIGNALS:
    void update();

private Q_SLOTS:
    void configureFilenameScheme();
    void updateView();
    void startRename();
    void renameFile();

private:
    void slotButtonClicked(int button);
    void readOptions();
    Device * getDevice(QWidget *p=0);
    void finish(bool ok);

private:
    FilenameSchemeDialog *schemeDlg;
    QList<Song> origSongs;
    QString deviceUdi;
    int index;
    bool autoSkip;
    bool paused;
    bool updated;
    Device::Options opts;
};

#endif
