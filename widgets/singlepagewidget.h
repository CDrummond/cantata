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

#ifndef SINGLE_PAGE_WIDGET_H
#define SINGLE_PAGE_WIDGET_H

#include <QWidget>
#include "mpd-interface/song.h"
#include "gui/page.h"
#include "widgets/itemview.h"
#include "mpd-interface/mpdconnection.h"

class Action;

class SinglePageWidget : public QWidget, public Page
{
    Q_OBJECT
public:
    enum {
        ReplacePlayQueue  = 0x01,
        AppendToPlayQueue = 0x02,
        Refresh           = 0x04,

        All = AppendToPlayQueue|ReplacePlayQueue|Refresh
    };

    typedef QPair<QString, int> MenuItem;
    static const char *constValProp;
    static QList<QAction *> createActions(const QList<MenuItem> &values,int currentVal, QWidget *parent, const char *slot);
    static Action * createMenuGroup(const QString &name, const QList<QAction *> actions, QWidget *parent);
    static Action * createMenuGroup(const QString &name, const QList<MenuItem> &values, int currentVal, QWidget *parent, const char *slot);

    SinglePageWidget(QWidget *p);
    ~SinglePageWidget() override { }
    void addWidget(QWidget *w);
    virtual void setView(int v);
    ItemView::Mode viewMode() const { return view->viewMode(); }
    void focusSearch() override;
    void init(int flags=All, const QList<QWidget *> &leftXtra=QList<QWidget *>(), const QList<QWidget *> &rightXtra=QList<QWidget *>());
    virtual QStringList selectedFiles(bool allowPlaylists=false) const { Q_UNUSED(allowPlaylists); return QStringList(); }
    QList<Song> selectedSongs(bool allowPlaylists=false) const override { Q_UNUSED(allowPlaylists); return QList<Song>(); }
    void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priority=0, bool decreasePriority=false) override;
    Song coverRequest() const override { return Song(); }
    #ifdef ENABLE_DEVICES_SUPPORT
    void addSelectionToDevice(const QString &udi) override { Q_UNUSED(udi); }
    void deleteSongs() override { }
    #endif
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    QList<QAction *> createViewActions(QList<ItemView::Mode> modes);
    Action * createViewMenu(QList<ItemView::Mode> modes);
    Action * refreshAct() { return refreshAction; }

public Q_SLOTS:
    virtual void doSearch() { }
    virtual void refresh() { }
    void controlActions() override;

Q_SIGNALS:
    void close();
    void searchItems();

    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files, int action, quint8 priority, bool decreasePriority);
    void addSongsToPlaylist(const QString &name, const QStringList &files);

private Q_SLOTS:
    void viewModeSelected();

protected:
    int btnFlags;
    ItemView *view;
    Action *refreshAction;
};

#endif

