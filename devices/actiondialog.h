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

#ifndef ACTION_DIALOG_H
#define ACTION_DIALOG_H

#include <KDE/KDialog>
#include "song.h"
#include "device.h"
#include "ui_actiondialog.h"

class KAction;
class KJob;
class DevicePropertiesDialog;

class ActionDialog : public KDialog, Ui::ActionDialog
{
    Q_OBJECT

    enum Mode
    {
        Copy,
        Remove
    };

public:
    ActionDialog(QWidget *parent, KAction *updateDbAct);
    void copy(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs);
    void remove(const QString &udi, const QList<Song> &songs);

private Q_SLOTS:
    void configureDest();
    void saveProperties(const QString &path, const Device::NameOptions &opts);
    void actionStatus(int status);
    void doNext();
    void removeSongResult(KJob *job);

private:
    void init(const QString &srcUdi, const QString &dstUdi, const QList<Song> &songs, Mode m);
    void slotButtonClicked(int button);
    void setPage(int page, const QString &msg=QString());
    QString formatSong(const Song &s);
    void refreshMpd();
    void removeSong(const Song &s);

private:
    Mode mode;
    QString sourceUdi;
    QString destUdi;
    QList<Song> songsToAction;
    QList<Song> actionedSongs;
    Song currentSong;
    DevicePropertiesDialog *propDlg;
    bool autoSkip;
    bool paused;
    bool performingAction;
    Device *currentDev;
    QString mpdDir;
    QString destFile;
    Device::NameOptions namingOptions;
    KAction *updateDbAction;
};

#endif
