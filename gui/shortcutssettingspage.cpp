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

#include "shortcutssettingspage.h"
#include "mediakeys.h"
#include "widgets/toolbutton.h"
#include "support/actioncollection.h"
#include "support/shortcutssettingswidget.h"
#include "widgets/basicitemdelegate.h"
#include "settings.h"
#include "widgets/icons.h"
#include "support/buddylabel.h"
#include "support/utils.h"
#include <QBoxLayout>
#include <QComboBox>
#include <QGroupBox>
#include <QFormLayout>
#include <QCheckBox>
#include <QProcess>

ShortcutsSettingsPage::ShortcutsSettingsPage(QWidget *p)
    : QWidget(p)
{
    QBoxLayout *lay=new QBoxLayout(QBoxLayout::TopToBottom, this);
    lay->setMargin(0);

    QHash<QString, ActionCollection *> map;
    map.insert("Cantata", ActionCollection::get());
    shortcuts = new ShortcutsSettingsWidget(map, this);
    shortcuts->view()->setAlternatingRowColors(false);
    shortcuts->view()->setItemDelegate(new BasicItemDelegate(shortcuts->view()));
    lay->addWidget(shortcuts);
}

void ShortcutsSettingsPage::load()
{
}

void ShortcutsSettingsPage::save()
{
    shortcuts->save();
}
