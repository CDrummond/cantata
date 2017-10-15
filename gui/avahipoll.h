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
    AvahiTimeout(const struct timeval *tv, AvahiTimeoutCallback callback, void *userdata);
    void updateTimeout(const struct timeval *tv);

private:
    QTimer m_timer;
    AvahiTimeoutCallback m_callback;
    void *m_userdata;

private slots:
    void timeout();
};


class AvahiWatch: public QObject
{
    Q_OBJECT

public:
    AvahiWatch(int fd, AvahiWatchEvent event, AvahiWatchCallback callback, void *userdata);
    void setEventType(AvahiWatchEvent event);
    AvahiWatchEvent previousEvent();

private:
    QScopedPointer<QSocketNotifier> m_notifier;
    AvahiWatchCallback m_callback;
    AvahiWatchEvent m_event;
    AvahiWatchEvent m_previousEvent;
    void *m_userdata;
    int m_fd;

private slots:
    void activated(int);
};


#endif //AVAHIPOLL_H
