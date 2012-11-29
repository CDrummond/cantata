/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "dirrequester.h"
#include "icon.h"
#include "localize.h"
#include <QtGui/QFileDialog>
#include <QtGui/QHBoxLayout>

DirRequester::DirRequester(QWidget *parent)
    : QWidget(parent)
{
    QHBoxLayout *layout=new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    edit=new LineEdit(this);
    button=new QToolButton(this);
    layout->addWidget(edit);
    layout->addWidget(button);
    button->setAutoRaise(true);
    button->setIcon(Icon("document-open"));
    connect(button, SIGNAL(clicked(bool)), SLOT(chooseDir()));
    connect(edit, SIGNAL(textChanged(const QString &)), SIGNAL(textChanged(const QString &)));
}

void DirRequester::chooseDir()
{
    QString dir=QFileDialog::getExistingDirectory(this, i18n("Select Folder"), edit->text());
    if (!dir.isEmpty()) {
        edit->setText(dir);
    }
}
