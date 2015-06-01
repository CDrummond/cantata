/*
 * Cantata Web
 *
 * Copyright (c) 2015 Craig Drummond <craig.p.drummond@gmail.com>
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


#include "apihandler.h"
#include "http/httpresponse.h"
#include <QJsonDocument>
#include <QTimer>

bool ApiHandler::dbgEnabled=false;

static QVariant toVariant(bool status)
{
    QVariantMap map;
    map["status"]=status;
    return map;
}

void ApiHandler::jsonHeaders(HttpResponse *response)
{
    response->setHeader("Content-Type", "application/json");
    response->setHeader("Cache-Control", "no-cache");
}

void ApiHandler::awaitResponse(HttpResponse *response)
{
    if (!timer) {
        timer=new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    }
    resp=response;
    timer->start(5000);
}

void ApiHandler::responseReceived(const QVariant &msg)
{
    if (!awaitingResponse()) {
        return;
    }
    jsonHeaders(resp);
    resp->write(QJsonDocument::fromVariant(msg).toJson());
    resp->complete();
    resp=0;
}

void ApiHandler::responseReceived(bool rv)
{
    if (!awaitingResponse()) {
        return;
    }
    setResponse(resp, rv);
    resp->complete();
    resp=0;
}

void ApiHandler::setResponse(HttpResponse *response, bool rv)
{
    jsonHeaders(response);
    response->write(QJsonDocument::fromVariant(toVariant(rv)).toJson());
}

void ApiHandler::timeout()
{
    responseReceived(false);
}
