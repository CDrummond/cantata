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

#ifndef MPD_BROWSE_PAGE_H
#define MPD_BROWSE_PAGE_H

#include "widgets/singlepagewidget.h"
#include "models/browsemodel.h"

class Action;

class MpdBrowsePage : public SinglePageWidget
{
    Q_OBJECT
public:
    MpdBrowsePage(QWidget *p);
    ~MpdBrowsePage() override;

    void setEnabled(bool e) { model.setEnabled(e); }
    bool isEnabled() const { return model.isEnabled(); }
    void load() { model.load(); }
    void clear() { model.clear(); }
    QString name() const { return model.name(); }
    QString title() const { return model.title(); }
    QString descr() const { return model.descr(); }
    const QIcon & icon() const { return model.icon(); }
    QStringList selectedFiles(bool allowPlaylists=false) const override;
    QList<Song> selectedSongs(bool allowPlaylists=false) const override;
    #ifdef ENABLE_DEVICES_SUPPORT
    void addSelectionToDevice(const QString &udi) override;
    void deleteSongs() override;
    #endif
    void addSelectionToPlaylist(const QString &name=QString(), int action=MPDConnection::Append, quint8 priority=0, bool decreasePriority=false) override;
    void showEvent(QShowEvent *e) override;

Q_SIGNALS:
    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);

public Q_SLOTS:
    void itemDoubleClicked(const QModelIndex &);
    void openFileManager();

private Q_SLOTS:
    void updateToPlayQueue(const QModelIndex &idx, bool replace);
    void headerClicked(int level);

private:
    void doSearch() override { }
    void controlActions() override;

private:
    Action *browseAction;
    BrowseModel model;
};

#endif
