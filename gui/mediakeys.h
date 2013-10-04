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

#ifndef MEDIA_KEYS_H
#define MEDIA_KEYS_H

#include <qglobal.h>

class GnomeMediaKeys;
class QxtMediaKeys;
class MultiMediaKeysInterface;

class MediaKeys
{
public:
    enum InterfaceType {
        NoInterface,
        GnomeInteface,
        QxtInterface
    };

    static MediaKeys * self();
    static QString toString(InterfaceType i);
    static InterfaceType toIface(const QString &i);

    MediaKeys();
    ~MediaKeys();

    void load();
    void stop();

private:
    void enable(MultiMediaKeysInterface *iface);
    void disable(MultiMediaKeysInterface *iface);

private:
    #if !defined Q_OS_WIN && !defined Q_OS_MAC
    GnomeMediaKeys *gnome;
    #endif
    #if !defined Q_OS_MAC && QT_VERSION < 0x050000
    QxtMediaKeys *qxt;
    #endif
};

#endif
