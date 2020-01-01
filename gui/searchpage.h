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

#ifndef SEARCHPAGE_H
#define SEARCHPAGE_H

#include "widgets/singlepagewidget.h"
#include "models/mpdsearchmodel.h"
#include "models/searchproxymodel.h"

class Action;
class SqueezedTextLabel;

class SearchPage : public SinglePageWidget
{
    Q_OBJECT

public:
    SearchPage(QWidget *p);
    ~SearchPage() override;

    void refresh() override;
    void clear();
    void setView(int mode) override;
    void showEvent(QShowEvent *e) override;
    QStringList selectedFiles(bool allowPlaylists=false) const override;
    QList<Song> selectedSongs(bool allowPlaylists=false) const override;
    #ifdef ENABLE_DEVICES_SUPPORT
    void addSelectionToDevice(const QString &udi) override;
    void deleteSongs() override;
    #endif
    void setSearchCategory(const QString &cat);

Q_SIGNALS:
    void addToDevice(const QString &from, const QString &to, const QList<Song> &songs);
    void deleteSongs(const QString &from, const QList<Song> &songs);
    void locate(const QList<Song> &songs);

public Q_SLOTS:   
    void itemDoubleClicked(const QModelIndex &);
    void setSearchCategories();
    void statsUpdated(int songs, quint32 time);
    void locateSongs();

private:
    void doSearch() override;
    void controlActions() override;

private:
    enum State
    {
        State_ComposerSupported  = 0x01,
        State_CommmentSupported  = 0x02,
        State_PerformerSupported = 0x04,
        State_ModifiedSupported  = 0x08,
        State_OrigDateSupported  = 0x10
    };

    int state;
    MpdSearchModel model;
    SearchProxyModel proxy;
    Action *locateAction;
    SqueezedTextLabel *statsLabel;
};

#endif
