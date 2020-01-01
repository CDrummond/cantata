/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "stackedpagewidget.h"
#include "mpd-interface/song.h"
#include "gui/page.h"
#include <QMap>

class QIcon;
class SelectorButton;
class SizeWidget;
class QLabel;
class Configuration;

class MultiPageWidget : public StackedPageWidget
{
    Q_OBJECT

    struct Entry
    {
        Entry(SelectorButton *b=nullptr, QWidget *p=nullptr) : btn(b), page(p) { }
        SelectorButton *btn;
        QWidget *page;
    };

public:
    MultiPageWidget(QWidget *p);
    ~MultiPageWidget() override;

    void load(Configuration &config);
    void save(Configuration &config) const;
    void setInfoText(const QString &text);
    void addPage(const QString &name, const QString &icon, const QString &text, const QString &subText, QWidget *widget);
    void addPage(const QString &name, const QIcon &icon, const QString &text, const QString &subText, QWidget *widget);
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
    QMap<QString, Entry> entries;
};

#endif
