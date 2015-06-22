/*
 * Cantata
 *
 * Copyright (c) 2011-2015 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef MULTI_PAGE_WIDGET_H
#define MULTI_PAGE_WIDGET_H

#include <QStackedWidget>
#include <QMap>
#include "mpd-interface/song.h"
#include "gui/page.h"

class Icon;
class SelectorButton;
class SizeWidget;
class QLabel;

class MultiPageWidget : public QStackedWidget, public Page
{
    Q_OBJECT

    struct Entry
    {
        Entry(SelectorButton *b=0, QWidget *p=0) : btn(b), page(p) { }
        SelectorButton *btn;
        QWidget *page;
    };

public:
    MultiPageWidget(QWidget *p);
    virtual ~MultiPageWidget();

    void setView(int v);
    void focusSearch();
    QStringList selectedFiles(bool allowPlaylists) const;
    QList<Song> selectedSongs(bool allowPlaylists) const;
    void addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty);
    void setInfoText(const QString &text);
    void addPage(const QString &name, const QString &icon, const QString &text, const QString &subText, QWidget *widget);
    void addPage(const QString &name, const Icon &icon, const QString &text, const QString &subText, QWidget *widget);
    void removePage(const QString &name);
    void sortItems();
    bool onMainPage() const { return mainPage==currentWidget(); }

public Q_SLOTS:
    void updatePageSubText(const QString &name, const QString &text);
    void showMainView();

private Q_SLOTS:
    void setPage();

private:
    QWidget *mainPage;
    QWidget *view;
    QLabel *infoLabel;
    SizeWidget *sizer;
    QMap<QString, Entry> entries;
};

#endif
