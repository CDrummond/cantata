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

#ifndef _SYNC_DIALOG_H_
#define _SYNC_DIALOG_H_

#include "support/dialog.h"
#include "mpd-interface/song.h"
#include "deviceoptions.h"

class Device;
class SyncCollectionWidget;
class SqueezedTextLabel;

class SyncDialog : public Dialog
{
    Q_OBJECT

    enum State {
        State_Lists,
        State_CopyToDevice,
        State_CopyToLib
    };

public:
    static int instanceCount();

    SyncDialog(QWidget *parent);
    ~SyncDialog() override;

    void sync(const QString &udi);

private Q_SLOTS:
    void copy(const QList<Song> &songs);
    void librarySongs(const QList<Song> &songs, double pc);
    void selectionChanged();
    void configure();
    void saveProperties(const QString &path, const DeviceOptions &opts);

private:
    void updateSongs();
    void slotButtonClicked(int button) override;
    Device * getDevice();

private:
    State state;
    SqueezedTextLabel *statusLabel;
    QString devUdi;
    Device *currentDev;
    SyncCollectionWidget *devWidget;
    SyncCollectionWidget *libWidget;
    QSet<Song> libSongs;
    DeviceOptions libOptions;
};

#endif
