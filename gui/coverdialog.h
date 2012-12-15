/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef COVER_DIALOG_H
#define COVER_DIALOG_H

#include "config.h"
#include "dialog.h"
#include "song.h"
#include <QtCore/QSet>

class ListWidget;
class QNetworkReply;
class QUrl;

class CoverDialog : public Dialog
{
    Q_OBJECT

public:
    static int instanceCount();

    CoverDialog(QWidget *parent);
    virtual ~CoverDialog();

    void show(const Song &song);

private Q_SLOTS:
    void queryJobFinished();
    void downloadJobFinished();

private:
    void sendQuery(const QString &query);
    QNetworkReply * downloadImage(const QString &url);
    void cancelQuery();
    void sendQueryRequest(const QUrl &url);
    void parseLstFmQueryResponse(const QString &resp);

private:
    ListWidget *list;
    QSet<QNetworkReply *> currentQuery;
};

#endif
