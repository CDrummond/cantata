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

#ifndef HTTP_STREAM_H
#define HTTP_STREAM_H

#include <QObject>

#if QT_VERSION < 0x050000
#include <phonon/mediaobject.h>
#else
#include <QtMultimedia/QMediaPlayer>
#endif

class HttpStream : public QObject
{
    Q_OBJECT
    
public:
    HttpStream(QObject *p);
    virtual ~HttpStream() { }
    
public Q_SLOTS:
    void setEnabled(bool e);
    
private Q_SLOTS:
    void updateStatus();
    void streamUrl(const QString &url);
    
private:
    bool enabled;
    #if QT_VERSION < 0x050000
    Phonon::MediaObject *player;
    #else
    QMediaPlayer *player;
    #endif    
};

#endif

