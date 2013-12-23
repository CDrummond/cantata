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

#ifndef MULTI_MEDIA_KEYS_INTERFACE_H
#define MULTI_MEDIA_KEYS_INTERFACE_H

#include <QObject>

class MultiMediaKeysInterface : public QObject
{
    Q_OBJECT
public:
    MultiMediaKeysInterface(QObject *p) : QObject(p), enabled(false) { }
    ~MultiMediaKeysInterface() { }

    void setEnabled(bool e) { activate(e); enabled=e; }
    bool isEnabled() const { return enabled; }

private:
    virtual void activate(bool a)=0;

Q_SIGNALS:
    void playPause();
    void stop();
    void next();
    void previous();

protected:
    bool enabled;
};

#endif
