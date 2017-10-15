/*
 *Copyright (C) <2017>  Alex B
 *
 *This program is free software: you can redistribute it and/or modify
 *it under the terms of the GNU General Public License as published by
 *the Free Software Foundation, either version 2 of the License, or
 *(at your option) any later version.
 *
 *This program is distributed in the hope that it will be useful,
 *but WITHOUT ANY WARRANTY; without even the implied warranty of
 *MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *GNU General Public License for more details.
 *
 *You should have received a copy of the GNU General Public License
 *along with this program.  If not, see <http://www.gnu.org/licenses/>
 */

#ifndef AVAHIPOLL_H
#define AVAHIPOLL_H

#include <avahi-common/watch.h>
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>

class AvahiTimeout: public QObject
{
    Q_OBJECT

public:
    AvahiTimeout(const struct timeval *tv, AvahiTimeoutCallback cb, void *ud);
    void updateTimeout(const struct timeval *tv);

private:
    QTimer timer;
    AvahiTimeoutCallback callback;
    void *userData;

private Q_SLOTS:
    void timeout();
};

class AvahiWatch: public QObject
{
    Q_OBJECT

public:
    AvahiWatch(int f, AvahiWatchEvent ev, AvahiWatchCallback cb, void *ud);
    void setEventType(AvahiWatchEvent event);
    AvahiWatchEvent previousEvent();

private:
    QScopedPointer<QSocketNotifier> notifier;
    AvahiWatchCallback callback;
    AvahiWatchEvent event;
    AvahiWatchEvent prevEvent;
    void *userData;
    int fd;

private Q_SLOTS:
    void activated(int fd);
};

#endif //AVAHIPOLL_H
