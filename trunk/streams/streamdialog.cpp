/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QValidator>
#include <QUrl>
#include "streamdialog.h"
#include "mainwindow.h"
#include "settings.h"
#include "streamsmodel.h"
#include "localize.h"
#include "icons.h"
#include "mpdconnection.h"
#include "buddylabel.h"

class NameValidator : public QValidator
{
    public:

    NameValidator(QObject *parent) : QValidator(parent) { }

    State validate(QString &input, int &) const
    {
        return input.contains("#") ? Invalid : Acceptable;
    }
};

StreamDialog::StreamDialog(QWidget *parent, bool addToPlayQueue)
    : Dialog(parent)
    , saveCombo(0)
    , urlHandlers(MPDConnection::self()->urlHandlers())
{
    QWidget *wid = new QWidget(this);
    QFormLayout *layout = new QFormLayout(wid);

    layout->setMargin(0);
    if (addToPlayQueue) {
        urlEntry = new LineEdit(wid);
        saveCombo=new QComboBox(wid);
        nameEntry = new LineEdit(wid);
    } else {
        nameEntry = new LineEdit(wid);
        urlEntry = new LineEdit(wid);
    }
    nameEntry->setValidator(new NameValidator(this));
    statusText = new QLabel(this);

    urlEntry->setMinimumWidth(300);
    nameLabel=new BuddyLabel(i18n("Name:"), wid, nameEntry);
    BuddyLabel *urlLabel=new BuddyLabel(i18n("URL:"), wid, urlEntry);

    int row=0;

    if (addToPlayQueue) {
        saveCombo->addItem(i18n("Just add to play queue, do not save"));
        saveCombo->addItem(i18n("Add to play queue, and save to favourites"));
        saveCombo->setCurrentIndex(0);
        saveCombo->setEnabled(StreamsModel::self()->isFavoritesWritable());
        layout->setWidget(row, QFormLayout::LabelRole, urlLabel);
        layout->setWidget(row++, QFormLayout::FieldRole, urlEntry);
        layout->setWidget(row++, QFormLayout::FieldRole, saveCombo);
        connect(saveCombo, SIGNAL(activated(int)), SLOT(saveComboChanged()));
        setWidgetVisiblity();
    }
    layout->setWidget(row, QFormLayout::LabelRole, nameLabel);
    layout->setWidget(row++, QFormLayout::FieldRole, nameEntry);
    if (!addToPlayQueue) {
        layout->setWidget(row, QFormLayout::LabelRole, urlLabel);
        layout->setWidget(row++, QFormLayout::FieldRole, urlEntry);
    }

    layout->setWidget(row++, QFormLayout::SpanningRole, statusText);
    setCaption(i18n("Add Stream"));
    setMainWidget(wid);
    setButtons(Ok|Cancel);
    enableButton(Ok, false);
    connect(nameEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    connect(urlEntry, SIGNAL(textChanged(const QString &)), SLOT(changed()));
    if (addToPlayQueue) {
        urlEntry->setFocus();
    } else {
        nameEntry->setFocus();
    }
    resize(400, 100);
}

void StreamDialog::setEdit(const QString &editName, const QString &editUrl)
{
    setCaption(i18n("Edit Stream"));
    enableButton(Ok, false);
    prevName=editName;
    prevUrl=editUrl;
    nameEntry->setText(editName);
    urlEntry->setText(editUrl);
}

void StreamDialog::saveComboChanged()
{
    setWidgetVisiblity();
    changed();
}

void StreamDialog::setWidgetVisiblity()
{
    bool s=save();
    nameEntry->setVisible(s);
    nameLabel->setVisible(s);
    QApplication::processEvents();
    adjustSize();
}

void StreamDialog::changed()
{
    QString u=url();
    bool validProtocol=u.isEmpty() || urlHandlers.contains(QUrl(u).scheme()) || urlHandlers.contains(u);
    bool enableOk=false;

    if (!save()) {
        enableOk=!u.isEmpty();
    } else {
        QString n=name();
        enableOk=!n.isEmpty() && !u.isEmpty() && (n!=prevName || u!=prevUrl);
        statusText->setText(validProtocol ? QString() : i18n("<i><b>ERROR:</b> Invalid protocol</i>"));
    }
    enableOk=enableOk && validProtocol;
    enableButton(Ok, enableOk);
}
