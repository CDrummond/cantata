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

#include "pathrequester.h"
#include "icon.h"
#include "monoicon.h"
#include "utils.h"
#include <QFileDialog>
#include <QHBoxLayout>

static QIcon icon;

PathRequester::PathRequester(QWidget *parent)
    : QWidget(parent)
    , dirMode(true)
{
    if (icon.isNull()) {
        icon = MonoIcon::icon(FontAwesome::foldero, Utils::monoIconColor());
    }
    QHBoxLayout *layout=new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    edit=new LineEdit(this);
    btn=new FlatToolButton(this);
    layout->addWidget(edit);
    layout->addWidget(btn);
    btn->setAutoRaise(true);
    btn->setIcon(icon);
    connect(btn, SIGNAL(clicked(bool)), SLOT(choose()));
    connect(edit, SIGNAL(textChanged(const QString &)), SIGNAL(textChanged(const QString &)));
}

void PathRequester::choose()
{
    QString item=dirMode
                    ? QFileDialog::getExistingDirectory(this, tr("Select Folder"), text())
                    : QFileDialog::getOpenFileName(this, tr("Select File"), Utils::getDir(text()), filter);
    if (!item.isEmpty()) {
        setText(item);
    }
}

void PathRequester::setEnabled(bool e)
{
    edit->setEnabled(e);
    btn->setEnabled(e);
}

#include "moc_pathrequester.cpp"
