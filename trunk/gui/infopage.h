/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* BE::MPC Qt4 client for MPD
 * Copyright (C) 2010-2011 Thomas Luebking <thomas.luebking@web.de>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License version 2 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef INFOPAGE_H
#define INFOPAGE_H

#include <QWidget>
#include "song.h"

class QTextBrowser;
class QAction;

class InfoPage : public QWidget
{
    Q_OBJECT

public:
    InfoPage(QWidget *parent);
    virtual ~InfoPage() { }
    void update(const Song &s);

private:
    void fetchInfo(bool useCache=true);
    void askGoogle(const QString &query, bool useCache=true);
    void fetchWiki(QString query);

private Q_SLOTS:
    void refresh();
    void changeView();
    void gg2wp(const QString &answer);
    void setArtistWiki(const QString &artistWiki);

private:
    QAction *refreshAction;
    QTextBrowser *text;
    QComboBox *combo;
    QString lastWikiQuestion;
    Song song;
};

#endif
