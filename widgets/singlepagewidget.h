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

#ifndef SINGLE_PAGE_WIDGET_H
#define SINGLE_PAGE_WIDGET_H

#include <QWidget>
#include "mpd-interface/song.h"
#include "gui/page.h"

class ItemView;
class ToolButton;
class Action;

class SinglePageWidget : public QWidget, public Page
{
    Q_OBJECT
public:
    enum {
        ReplacePlayQueue = 0x01,
        AddToPlayQueue   = 0x02,
        Refresh          = 0x04,
        Configure        = 0x08,

        All = AddToPlayQueue|ReplacePlayQueue|Refresh|Configure
    };
    SinglePageWidget(QWidget *p);
    virtual void setView(int v);
    void focusSearch();
    void init(int flags=All, const QList<ToolButton *> &leftXtra=QList<ToolButton *>(), const QList<ToolButton *> &rightXtra=QList<ToolButton *>());
    virtual QStringList selectedFiles(bool allowPlaylists=false) const { Q_UNUSED(allowPlaylists); return QStringList(); }
    virtual QList<Song> selectedSongs(bool allowPlaylists=false) const { Q_UNUSED(allowPlaylists); return QList<Song>(); }
    void addSelectionToPlaylist(const QString &name, bool replace, quint8 priorty);
    virtual Song coverRequest() const { return Song(); }
    #ifdef ENABLE_DEVICES_SUPPORT
    virtual void addSelectionToDevice(const QString &udi) { Q_UNUSED(udi); }
    virtual void deleteSongs() { }
    #endif
    void showEvent(QShowEvent *e);

public Q_SLOTS:
    virtual void doSearch() { }
    virtual void refresh() { }
    virtual void configure() { }
    virtual void controlActions();

Q_SIGNALS:
    void close();
    void searchItems();

    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void add(const QStringList &files, bool replace, quint8 priorty);
    void addSongsToPlaylist(const QString &name, const QStringList &files);

protected:
    int btnFlags;
    ItemView *view;
    Action *refreshAction;
    Action *configureAction;
};

#endif

