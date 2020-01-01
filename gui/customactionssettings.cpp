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

#include "customactionssettings.h"
#include "customactions.h"
#include "support/messagebox.h"
#include "widgets/basicitemdelegate.h"
#include "widgets/icons.h"
#include "widgets/notelabel.h"
#include <QGridLayout>
#include <QFormLayout>
#include <QLabel>
#include <QSpacerItem>
#include <QPushButton>
#include <QHeaderView>

CustomActionDialog::CustomActionDialog(QWidget *p)
    : Dialog(p)
{
    QWidget *widget=new QWidget(this);
    QFormLayout *layout=new QFormLayout(widget);
    nameEntry=new LineEdit(widget);
    commandEntry=new LineEdit(widget);
    nameEntry->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    commandEntry->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
    layout->addRow(new QLabel(tr("Name:"), widget), nameEntry);
    layout->addRow(new QLabel(tr("Command:"), widget), commandEntry);
    layout->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    NoteLabel *note=new NoteLabel(widget);
    note->setText(tr("In the command line above, %f will be replaced with the file list and %d with the folder list. If neither are supplied, the the list of files will be appended to the command."));
    layout->setWidget(2, QFormLayout::SpanningRole, note);
    layout->setMargin(0);
    setButtons(Dialog::Ok|Dialog::Cancel);
    setMainWidget(widget);
    setMinimumWidth(400);
    ensurePolished();
    adjustSize();
}

bool CustomActionDialog::create()
{
    setCaption(tr("Add New Command"));
    nameEntry->setText(QString());
    commandEntry->setText(QString());
    return QDialog::Accepted==exec();
}

bool CustomActionDialog::edit(const QString &name, const QString &cmd)
{
    setCaption(tr("Edit Command"));
    nameEntry->setText(name);
    commandEntry->setText(cmd);
    return QDialog::Accepted==exec();
}

static inline void setResizeMode(QHeaderView *hdr, int idx, QHeaderView::ResizeMode mode)
{
    hdr->setSectionResizeMode(idx, mode);
}

CustomActionsSettings::CustomActionsSettings(QWidget *parent)
    : QWidget(parent)
    , dlg(nullptr)
{
    QGridLayout *layout=new QGridLayout(this);
    layout->setMargin(0);
    QLabel *label=new QLabel(tr("To have Cantata call external commands (e.g. to edit tags with another application), add an entry for the command below. When at least one command "
                                  "command is defined, a 'Custom Actions' entry will be added to the context menus in the Library, Folders, and Playlists views."), this);
    QFont f(Utils::smallFont(label->font()));
    f.setItalic(true);
    label->setFont(f);
    label->setWordWrap(true);
    layout->addWidget(label, 0, 0, 1, 2);
    tree=new QTreeWidget(this);
    add=new QPushButton(this);
    edit=new QPushButton(this);
    del=new QPushButton(this);
    layout->addWidget(tree, 1, 0, 4, 1);
    layout->addWidget(add, 1, 1, 1, 1);
    layout->addWidget(edit, 2, 1, 1, 1);
    layout->addWidget(del, 3, 1, 1, 1);
    layout->addItem(new QSpacerItem(1, 1, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding), 4, 1);

    add->setText(tr("Add"));
    edit->setText(tr("Edit"));
    del->setText(tr("Remove"));
    edit->setEnabled(false);
    del->setEnabled(false);

    tree->setHeaderLabels(QStringList() << tr("Name") << tr("Command"));
    tree->setAllColumnsShowFocus(true);
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree->setRootIsDecorated(false);
    tree->setSortingEnabled(true);
    setResizeMode(tree->header(), 0, QHeaderView::ResizeToContents);
    tree->header()->setStretchLastSection(true);
    tree->setAlternatingRowColors(false);
    tree->setItemDelegate(new BasicItemDelegate(this));
    connect(tree, SIGNAL(itemSelectionChanged()), this, SLOT(controlButtons()));
    connect(add, SIGNAL(clicked()), SLOT(addCommand()));
    connect(edit, SIGNAL(clicked()), SLOT(editCommand()));
    connect(del, SIGNAL(clicked()), SLOT(delCommand()));
}

CustomActionsSettings::~CustomActionsSettings()
{
}

void CustomActionsSettings::controlButtons()
{
    int selCount=tree->selectedItems().count();
    edit->setEnabled(1==selCount);
    del->setEnabled(selCount>0);
}

void CustomActionsSettings::load()
{
    for (const CustomActions::Command &cmd: CustomActions::self()->commandList()) {
        new QTreeWidgetItem(tree, QStringList() << cmd.name << cmd.cmd);
    }
}

void CustomActionsSettings::save()
{
    QList<CustomActions::Command> commands;

    for (int i=0; i<tree->topLevelItemCount(); ++i) {
        commands.append((CustomActions::Command(tree->topLevelItem(i)->text(0), tree->topLevelItem(i)->text(1))));
    }
    CustomActions::self()->set(commands);
}

void CustomActionsSettings::addCommand()
{
    if (!dlg) {
        dlg=new CustomActionDialog(this);
    }
    if (dlg->create() && !dlg->nameText().isEmpty() && !dlg->commandText().isEmpty()) {
        new QTreeWidgetItem(tree, QStringList() << dlg->nameText() << dlg->commandText());
    }
}

void CustomActionsSettings::editCommand()
{
    QList<QTreeWidgetItem *> items=tree->selectedItems();

    if (1!=items.count()) {
        return;
    }
    if (!dlg) {
        dlg=new CustomActionDialog(this);
    }
    QTreeWidgetItem *item=items.at(0);
    if (dlg->edit(item->text(0), item->text(1)) && !dlg->nameText().isEmpty() && !dlg->commandText().isEmpty()) {
        item->setText(0, dlg->nameText());
        item->setText(1, dlg->commandText());
    }
}

void CustomActionsSettings::delCommand()
{
    if (MessageBox::Yes==MessageBox::warningYesNo(this, tr("Remove the selected commands?"), QString(), GuiItem(tr("Remove")), StdGuiItem::cancel())) {
        for (QTreeWidgetItem *i: tree->selectedItems()) {
            delete i;
        }
    }
}

#include "moc_customactionssettings.cpp"
