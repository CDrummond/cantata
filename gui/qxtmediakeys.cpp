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

#include "qxtmediakeys.h"
#include "qxt/qxtglobalshortcut.h"

QxtMediaKeys::QxtMediaKeys(QObject *p)
    : MultiMediaKeysInterface(p)
{
}

bool QxtMediaKeys::activate()
{
    createShortcuts();
    return true; // Hmm... How to detect if this failed?
}

void QxtMediaKeys::deactivate()
{
    clear();
}

void QxtMediaKeys::createShortcuts()
{
    if (!shortcuts.isEmpty()) {
        return;
    }
    QxtGlobalShortcut *shortcut = new QxtGlobalShortcut(Qt::Key_MediaPlay, this);
    connect(shortcut, SIGNAL(activated()), this, SIGNAL(playPause()));
    shortcuts.append(shortcut);
    shortcut = new QxtGlobalShortcut(Qt::Key_MediaStop, this);
    connect(shortcut, SIGNAL(activated()), this, SIGNAL(stop()));
    shortcuts.append(shortcut);
    shortcut = new QxtGlobalShortcut(Qt::Key_MediaNext, this);
    connect(shortcut, SIGNAL(activated()), this, SIGNAL(next()));
    shortcuts.append(shortcut);
    shortcut = new QxtGlobalShortcut(Qt::Key_MediaPrevious, this);
    connect(shortcut, SIGNAL(activated()), this, SIGNAL(previous()));
    shortcuts.append(shortcut);
}

void QxtMediaKeys::clear()
{
    if (!shortcuts.isEmpty()) {
        qDeleteAll(shortcuts);
        shortcuts.clear();
    }
}

