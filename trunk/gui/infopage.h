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

#ifndef INFOPAGE_H
#define INFOPAGE_H

#include <QtGui/QWidget>
#include "song.h"

class QComboBox;
class WebView;
class QNetworkRequest;
class QUrl;

class InfoPage : public QWidget
{
    Q_OBJECT

public:
    InfoPage(QWidget *parent);
    virtual ~InfoPage() { }

    void save();
    void update(const Song &s);

private:
    void fetchInfo();
    void askGoogle(const QString &query);
//     void fetchWiki(QString query);

private Q_SLOTS:
    void changeView();
    void googleAnswer(const QString &answer);
    void downloadRequested(const QNetworkRequest &);
    void downloadingFinished();
    #ifdef ENABLE_KDE_SUPPORT
    void updateFonts();
    #endif
    void handleReply();

private:
    void get(const QUrl &url);

private:
    WebView *view;
    QComboBox *combo;
    QString lastWikiQuestion;
    bool iHaveAskedForArtist;
    Song song;
};

#endif
