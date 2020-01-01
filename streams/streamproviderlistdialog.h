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

#ifndef STREAM_PROVIDER_LIST_DIALOG_H
#define STREAM_PROVIDER_LIST_DIALOG_H

#include "support/dialog.h"
#include "support/icon.h"
#include <QSet>
#include <QHash>

class NetworkJob;
class QTreeWidget;
class QTreeWidgetItem;
class QProgressBar;
class SqueezedTextLabel;
class StreamsSettings;
class Spinner;
class MessageOverlay;
class QIODevice;

class StreamProviderListDialog : public Dialog
{
    Q_OBJECT

public:
    StreamProviderListDialog(StreamsSettings *parent);
    ~StreamProviderListDialog() override;
    void show(const QSet<QString> &installed);

private Q_SLOTS:
    void getProviderList();
    void jobFinished();
    void itemChanged(QTreeWidgetItem *itm, int col);

private:
    void readProviders(QIODevice *dev);
    void slotButtonClicked(int button) override;
    void updateView(bool unCheck=false);
    void doNext();
    void setState(bool downloading);

private:
    QIcon def;
    QIcon installed;
    QIcon updateable;
    StreamsSettings *p;
    NetworkJob *job;
    Spinner *spinner;
    MessageOverlay *msgOverlay;
    QTreeWidget *tree;
    QProgressBar *progress;
    QString updateText;
    SqueezedTextLabel *statusText;
    QHash<QString, QString> installedProviders;
    QList<QTreeWidgetItem *> processItems;
    QSet<QTreeWidgetItem *> checkedItems;
};

#endif
