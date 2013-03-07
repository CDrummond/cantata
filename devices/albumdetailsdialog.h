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

#ifndef ALBUMDETAILSDIALOG_H
#define ALBUMDETAILSDIALOG_H

#include "ui_albumdetails.h"
#include "dialog.h"
#include "song.h"
#include "covers.h"

class AudioCdDevice;
class QTreeWidgetItem;

class AlbumDetailsDialog : public Dialog, Ui::AlbumDetails
{
    Q_OBJECT

public:
    static int instanceCount();

    AlbumDetailsDialog(QWidget *parent);
    virtual ~AlbumDetailsDialog();
    void show(AudioCdDevice *dev);

private Q_SLOTS:
    void hideArtistColumn(bool hide);
    void applyVa();
    void revertVa();
    void capitalise();
    void adjustTrackNumbers();
    void coverSelected(const QImage &img, const QString &fileName);

private:
    void slotButtonClicked(int button);
    Song toSong(QTreeWidgetItem *i);
    void update(QTreeWidgetItem *i, const Song &s);
    void setCover();
    bool eventFilter(QObject *object, QEvent *event);

private:
    QString udi;
    bool pressed;
    Covers::Image coverImage;
};

#endif
