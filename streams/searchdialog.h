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

#ifndef SEARCH_DIALOG_H
#define SEARCH_DIALOG_H

#include "dialog.h"
#include "song.h"
#include "ui_searchdialog.h"
#include "covers.h"
#include <QMap>
#include <QList>

class QNetworkReply;
class Spinner;
class StreamsPage;
class QListWidgetItem;

class SearchDialog : public Dialog, public Ui::SearchDialog
{
    Q_OBJECT

public:
    static int instanceCount();

    SearchDialog(StreamsPage *parent);
    virtual ~SearchDialog();

Q_SIGNALS:
    void add(const QStringList &streams, bool replace, quint8 priorty);

private Q_SLOTS:
    void performSearch();
    void jobFinished();
    void imageJobFinished();
    void checkStatus();
    
private:
    void cancelSearch();
    void slotButtonClicked(int button);

private:
    Spinner *spinner;
    QNetworkReply *job;
    QList<QNetworkReply *> imageRequests;
    QMap<QString, QListWidgetItem *> itemUrlMap;
};

#endif
