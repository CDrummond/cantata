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

#include <QFormLayout>
#include <QIcon>
#include <QUrl>
#include <QApplication>
#include "streamdialog.h"
#include "gui/settings.h"
#include "models/streamsmodel.h"
#include "widgets/icons.h"
#include "mpd-interface/mpdconnection.h"
#include "support/buddylabel.h"
#include "support/utils.h"
#include "config.h"

StreamDialog::StreamDialog(QWidget *parent, bool addToPlayQueue)
    : Dialog(parent)
    , saveCheckbox(nullptr)
    , urlHandlers(MPDConnection::self()->urlHandlers())
{
    QWidget *wid = new QWidget(this);
    QFormLayout *layout = new QFormLayout(wid);

    layout->setMargin(0);
    urlEntry = new LineEdit(wid);
    nameEntry = new LineEdit(wid);
    if (addToPlayQueue) {
        saveCheckbox=new QCheckBox(tr("Add stream to favourites"), wid);
    }
    statusText = new QLabel(this);

    urlEntry->setMinimumWidth(300);
    nameLabel=new BuddyLabel(tr("Name:"), wid, nameEntry);
    BuddyLabel *urlLabel=new BuddyLabel(tr("URL:"), wid, urlEntry);

    int row=0;
    layout->setWidget(row, QFormLayout::LabelRole, urlLabel);
    layout->setWidget(row++, QFormLayout::FieldRole, urlEntry);
    layout->setWidget(row, QFormLayout::LabelRole, nameLabel);
    layout->setWidget(row++, QFormLayout::FieldRole, nameEntry);
    if (addToPlayQueue) {
        saveCheckbox->setChecked(false);
        layout->setWidget(row++, QFormLayout::FieldRole, saveCheckbox);
        connect(saveCheckbox, SIGNAL(toggled(bool)), SLOT(changed()));
    }
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    layout->setWidget(row++, QFormLayout::SpanningRole, statusText);
    setCaption(tr("Add Stream"));
    setMainWidget(wid);
    setButtons(Ok|Cancel);
    enableButton(Ok, false);
    connect(nameEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    connect(urlEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    urlEntry->setFocus();
    resize(400, 100);
}

void StreamDialog::setEdit(const QString &editName, const QString &editUrl)
{
    setCaption(tr("Edit Stream"));
    enableButton(Ok, false);
    prevName=editName;
    prevUrl=editUrl;
    nameEntry->setText(editName);
    urlEntry->setText(editUrl);
}

void StreamDialog::changed()
{
    QString u=url();
    bool urlOk=u.length()>5 && u.contains(QLatin1String("://"));
    bool validProtocol=u.isEmpty() || urlHandlers.contains(QUrl(u).scheme()) || urlHandlers.contains(u);
    bool enableOk=false;

    if (!save()) {
        enableOk=urlOk;
    } else {
        QString n=name();
        enableOk=!n.isEmpty() && urlOk && (n!=prevName || u!=prevUrl);
    }
    statusText->setText(validProtocol || !urlOk ? QString() : tr("<i><b>ERROR:</b> Invalid protocol</i>"));
    enableOk=enableOk && validProtocol;
    enableButton(Ok, enableOk);
}

#include "moc_streamdialog.cpp"
