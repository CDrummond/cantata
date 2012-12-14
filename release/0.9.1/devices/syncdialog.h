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

#ifndef _SYNC_DIALOG_H_
#define _SYNC_DIALOG_H_

#include "dialog.h"
#include "song.h"

class Device;
class SyncCollectionWidget;

class SyncDialog : public Dialog
{
    Q_OBJECT

public:
    static int instanceCount();

    SyncDialog(QWidget *parent);
    virtual ~SyncDialog();

    void sync(const QString &udi);

private Q_SLOTS:
    void copy(const QList<Song> &songs);
    bool updateSongs(bool firstRun=false);

private:
    Device * getDevice();

private:
    QString devUdi;
    Device *currentDev;
    SyncCollectionWidget *devWidget;
    SyncCollectionWidget *libWidget;
};

#endif
