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

#include "contextengine.h"
#include "wikipediaengine.h"
#include "wikipediasettings.h"
#include <QNetworkReply>

ContextEngine * ContextEngine::create(QObject *parent)
{
    return new WikipediaEngine(parent);
}

ContextSettings * ContextEngine::settings(QWidget *parent)
{
    return new WikipediaSettings(parent);
}

ContextEngine::ContextEngine(QObject *p)
    : QObject(p)
    , job(0)
{
}

ContextEngine::~ContextEngine()
{
    cancel();
}

void ContextEngine::cancel()
{
    if (job) {
        job->deleteLater();
        job=0;
    }
}

QNetworkReply * ContextEngine::getReply(QObject *obj)
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(obj);
    if (!reply) {
        return 0;
    }

    reply->deleteLater();
    if (reply!=job) {
        return 0;
    }
    job=0;
    return reply;
}
