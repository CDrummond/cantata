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

#ifndef API_HANDLER_H
#define API_HANDLER_H

#include <QObject>
#include "http/httprequesthandler.h"

class HttpRequest;
class HttpResponse;
class QTimer;

class ApiHandler : public QObject, public HttpRequestHandler
{
    Q_OBJECT
public:
    static void jsonHeaders(HttpResponse *response);
    static void enableDebug() { dbgEnabled=true; }
    static bool debugEnabled() { return dbgEnabled; }

    ApiHandler(QObject *p=0) : QObject(p), resp(0), timer(0) { }

protected:
    void awaitResponse(HttpResponse *response);
    void responseReceived(const QVariant &msg);
    void responseReceived(bool rv);
    void setResponse(HttpResponse *response, bool rv);
    bool awaitingResponse() { return 0!=resp; }

private Q_SLOTS:
    void timeout();

private:
    HttpResponse *resp;
    QTimer *timer;
    static bool dbgEnabled;
};

#endif
