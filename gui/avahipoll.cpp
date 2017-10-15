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

#include "avahipoll.h"
#include <avahi-common/timeval.h>
#include <QThread>

static AvahiWatch * avahiWatchNew(const AvahiPoll *ap, int fd, AvahiWatchEvent event, AvahiWatchCallback callback, void *userdata)
{
    return new AvahiWatch(fd, event, callback, userdata);
}

static void avahiWatchUpdate(AvahiWatch *w, AvahiWatchEvent event)
{
    w->setEventType(event);
}

static AvahiWatchEvent avahiWatchGetEvents(AvahiWatch *w)
{
    return w->previousEvent();
}

static void avahiWatchFree(AvahiWatch *w)
{
    (w->thread() == QThread::currentThread()) ? delete w : w->deleteLater();
}

static AvahiTimeout * avahiTimeoutNew(const AvahiPoll *p, const struct timeval *tv, AvahiTimeoutCallback callback, void *userdata)
{
    Q_UNUSED(p)
    return new AvahiTimeout(tv, callback, userdata);
}

static void avahiTimeoutUpdate(AvahiTimeout *t, const struct timeval *tv)
{
    t->updateTimeout(tv);
}

static void avahiTimeoutFree(AvahiTimeout *t)
{
    (t->thread() == QThread::currentThread()) ? delete t : t->deleteLater();
}

const AvahiPoll * getAvahiPoll(void)
{
    static const AvahiPoll avahiPoll =
    {
        nullptr,
        avahiWatchNew,
        avahiWatchUpdate,
        avahiWatchGetEvents,
        avahiWatchFree,
        avahiTimeoutNew,
        avahiTimeoutUpdate,
        avahiTimeoutFree
    };

    return &avahiPoll;
}

AvahiTimeout::AvahiTimeout(const struct timeval *tv, AvahiTimeoutCallback callback, void *userdata)
    : m_callback(callback)
    , m_userdata(userdata)
{
    connect(&m_timer, &QTimer::timeout, this, &AvahiTimeout::timeout);
    updateTimeout(tv);
}

void AvahiTimeout::updateTimeout(const struct timeval *tv)
{
    if (0==tv) {
        m_timer.stop();
        return;
    }

    qint64 msecs = (avahi_age(tv)) / 1000;

    msecs = (msecs > 0) ? 0 : -msecs;

    m_timer.setInterval(msecs);
    m_timer.start();
}

void AvahiTimeout::timeout()
{
    m_timer.stop();
    m_callback(this, m_userdata);
}

AvahiWatch::AvahiWatch(int fd, AvahiWatchEvent event, AvahiWatchCallback callback, void *userdata)
    : m_notifier(0)
    , m_callback(callback)
    , m_event(event)
    , m_previousEvent(static_cast<AvahiWatchEvent>(0))
    , m_userdata(userdata)
    , m_fd(fd)
{
    setEventType(event);
}

void AvahiWatch::setEventType(AvahiWatchEvent event)
{
    m_event = event;

    switch(m_event) {
    case AVAHI_WATCH_IN : {
        m_notifier.reset(new QSocketNotifier(m_fd, QSocketNotifier::Read, this));
        connect(m_notifier.data(), &QSocketNotifier::activated, this, &AvahiWatch::activated);
        break;
    }
    case AVAHI_WATCH_OUT : {
        m_notifier.reset(new QSocketNotifier(m_fd, QSocketNotifier::Write, this));
        connect(m_notifier.data(), &QSocketNotifier::activated, this, &AvahiWatch::activated);
        break;
    }
    default:
        // should not be reached
        break;
    }
}

void AvahiWatch::activated(int fd)
{
    Q_UNUSED(fd)

    m_previousEvent = m_event;
    m_callback(this, m_fd, m_event, m_userdata);
    m_previousEvent = static_cast<AvahiWatchEvent>(0);
}

AvahiWatchEvent AvahiWatch::previousEvent()
{
    return m_previousEvent;
}
