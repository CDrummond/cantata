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

#ifndef FOLDERPAGE_H
#define FOLDERPAGE_H

#include "widgets/multipagewidget.h"
#include "models/browsemodel.h"
#include "mpdbrowsepage.h"

class MpdBrowsePage;
class LocalFolderBrowsePage;

class FolderPage : public MultiPageWidget
{
    Q_OBJECT
public:
    FolderPage(QWidget *p);
    ~FolderPage() override;

    // TODO: Act on all pages...
    void setEnabled(bool e) { mpdBrowse->setEnabled(e); }
    bool isEnabled() const { return mpdBrowse->isEnabled(); }

    MpdBrowsePage *mpd() { return mpdBrowse; }

    #ifdef ENABLE_DEVICES_SUPPORT
    void addSelectionToDevice(const QString &udi) override;
    #endif

private:
    MpdBrowsePage *mpdBrowse;
    LocalFolderBrowsePage *homeBrowse;
};

#endif
