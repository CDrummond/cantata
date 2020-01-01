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

#ifndef VIEW_H
#define VIEW_H

#include <QWidget>
#include <QSize>
#include <QLabel>
#include "mpd-interface/song.h"
#include "widgets/selectorlabel.h"

class QImage;
class Spinner;
class QNetworkReply;
class QLayoutItem;
class TextBrowser;
class Action;
class QStackedWidget;
class QMenu;

class View : public QWidget
{
    Q_OBJECT
public:
    static QString subTag;

    View(QWidget *p, const QStringList &views=QStringList());
    ~View() override;

    static QString encode(const QImage &img);
    static QString subHeader(const QString &str) { return "<"+subTag+">"+str+"</"+subTag+">"; }
    static void initHeaderTags();

    void clear();
    void setStandardHeader(const QString &h) { stdHeader=h; }
    void setHeader(const QString &str);
    void setPicSize(const QSize &sz);
    QSize picSize() const;
    QString createPicTag(const QImage &img, const QString &file);
    void showEvent(QShowEvent *e) override;
    void showSpinner(bool enableCancel=true);
    void hideSpinner(bool disableCancel=true);
    void setEditable(bool e, int index=0);
    void setPal(const QPalette &pal, const QColor &linkColor, const QColor &prevLinkColor);
    void addEventFilter(QObject *obj);
    virtual void update(const Song &s, bool force)=0;
    void setHtml(const QString &h, int index=0);
    int currentView() const { return selector ? selector->currentIndex() : -1; }
    void setCurrentView(int v) { selector->setCurrentIndex(v); }

Q_SIGNALS:
    void viewChanged();

protected Q_SLOTS:
    virtual void abort();
    void setScaleImage(bool s);

protected:
    Song currentSong;
    QString stdHeader;
    QLabel *header;
    bool needToUpdate;
    Spinner *spinner;
    Action *cancelJobAction;

    SelectorLabel *selector;
    QStackedWidget *stack;
    TextBrowser *text; // short-cut to first text item...
    QList<TextBrowser *> texts;
};

#endif
