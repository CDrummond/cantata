/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "ui_trackorganiser.h"
#include "songdialog.h"
#include "config.h"
#ifdef ENABLE_DEVICES_SUPPORT
#include "device.h"
#else
#include "deviceoptions.h"
#endif
#include <QSet>

class FilenameSchemeDialog;
class Action;

class TrackOrganiser : public SongDialog, Ui::TrackOrganiser
{
    Q_OBJECT

public:
    static int instanceCount();

    TrackOrganiser(QWidget *parent);
    virtual ~TrackOrganiser();

    void show(const QList<Song> &songs, const QString &udi, bool forceUpdate=false, const QSet<QString> &prevModifiedDirs=QSet<QString>());

Q_SIGNALS:
    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void update(const QSet<QString> &dirs);

private Q_SLOTS:
    void configureFilenameScheme();
    void updateView();
    void startRename();
    void renameFile();
    void controlRemoveAct();
    void removeItems();
    void showMopidyMessage();
    void setFilenameScheme(const QString &text);

private:
    void saveOptions();
    void slotButtonClicked(int button);
    void readOptions();
    #ifdef ENABLE_DEVICES_SUPPORT
    Device * getDevice(QWidget *p=0);
    #endif
    void doUpdate();
    void finish(bool ok);

private:
    FilenameSchemeDialog *schemeDlg;
    QList<Song> origSongs;
    QString deviceUdi;
    Action *removeAct;
    QSet<QString> modifiedDirs;
    int index;
    bool autoSkip;
    bool paused;
    bool updated;
    bool alwaysUpdate;
    DeviceOptions opts;
};

#endif
