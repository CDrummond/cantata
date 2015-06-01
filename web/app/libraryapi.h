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

#ifndef LIBRARY_API_H
#define LIBRARY_API_H

#include "apihandler.h"

class HttpRequest;
class HttpResponse;

class LibraryApi : public ApiHandler
{
    Q_OBJECT

public:
    LibraryApi(QObject *p=0);
    virtual ~LibraryApi();
    HandleStatus handle(HttpRequest *request, HttpResponse *response);

Q_SIGNALS:
    void add(const QStringList &files, bool replace, quint8 priority);
};

#endif
