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

#ifndef CURRENT_COVERW_H
#define CURRENT_COVERW_H

#include <QObject>
#include <QImage>
#include "mpd-interface/song.h"

class QPixmap;
class QTimer;

class CurrentCover : public QObject
{
    Q_OBJECT

public:
    static CurrentCover * self();
    CurrentCover();
    ~CurrentCover() override;

    void setEnabled(bool e);
    void update(const Song &s);
    const Song & song() const { return current; }
    bool isValid() const { return valid; }
    const QString & fileName() const { return coverFileName; }
    const QImage &image() const { return img; }

Q_SIGNALS:
    void coverImage(const QImage &img);
    void coverFile(const QString &name);

private Q_SLOTS:
    void coverRetrieved(const Song &s, const QImage &img, const QString &file);
    void setDefault();

private:
    const QImage & stdImage(bool stream);
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    void initIconThemes();
    QString findIcon(const QStringList &names);
    #endif

private:
    bool enabled;
    bool valid;
    Song current;
    mutable QImage img;
    QString coverFileName;
    QImage noStreamCover;
    QImage noCover;
    QString noStreamCoverFileName;
    QString noCoverFileName;
    QTimer *timer;
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    QStringList iconThemes;
    #endif
};

#endif
