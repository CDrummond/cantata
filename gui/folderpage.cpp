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

#include "folderpage.h"
#include "localfolderpage.h"
#include "support/configuration.h"
#include "widgets/icons.h"

FolderPage::FolderPage(QWidget *p)
    : MultiPageWidget(p)
{
    mpdBrowse=new MpdBrowsePage(this);
    addPage(mpdBrowse->name(), mpdBrowse->icon(), mpdBrowse->title(), mpdBrowse->descr(), mpdBrowse);

    homeBrowse=new LocalFolderBrowsePage(true, this);
    addPage(homeBrowse->name(), homeBrowse->icon(), homeBrowse->title(), homeBrowse->descr(), homeBrowse);

    LocalFolderBrowsePage *rootBrowse=new LocalFolderBrowsePage(false, this);
    addPage(rootBrowse->name(), rootBrowse->icon(), rootBrowse->title(), rootBrowse->descr(), rootBrowse);

    Configuration config(metaObject()->className());
    load(config);
}

FolderPage::~FolderPage()
{
    Configuration config(metaObject()->className());
    save(config);
}

#ifdef ENABLE_DEVICES_SUPPORT
void FolderPage::addSelectionToDevice(const QString &udi)
{
    if (mpdBrowse==currentWidget()) {
        mpdBrowse->addSelectionToDevice(udi);
    }
}
#endif

#include "moc_folderpage.cpp"
