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

#include "dynamicrulesdialog.h"
#include "dynamicruledialog.h"
#include "dynamic.h"
#include <QtGui/QIcon>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KMessageBox>
#else
#include <QtGui/QDialogButtonBox>
#include <QtGui/QMessageBox>
#endif

static QString translateStr(const QString &key)
{
    if (Dynamic::constArtistKey==key) {
        #ifdef ENABLE_KDE_SUPPORT
        return i18n("Artist");
        #else
        return QObject::tr("Artist");
        #endif
    } else if (Dynamic::constAlbumArtistKey==key) {
        #ifdef ENABLE_KDE_SUPPORT
        return i18n("AlbumArtist");
        #else
        return QObject::tr("AlbumArtist");
        #endif
    } else if (Dynamic::constAlbumKey==key) {
        #ifdef ENABLE_KDE_SUPPORT
        return i18n("Album");
        #else
        return QObject::tr("Album");
        #endif
    } else if (Dynamic::constTitleKey==key) {
        #ifdef ENABLE_KDE_SUPPORT
        return i18n("Title");
        #else
        return QObject::tr("Title");
        #endif
    } else if (Dynamic::constGenreKey==key) {
        #ifdef ENABLE_KDE_SUPPORT
        return i18n("Genre");
        #else
        return QObject::tr("Genre");
        #endif
    } else if (Dynamic::constDateKey==key) {
        #ifdef ENABLE_KDE_SUPPORT
        return i18n("Date");
        #else
        return QObject::tr("Date");
        #endif
    } else {
        return key;
    }
}

static void update(QListWidgetItem *i, const Dynamic::Rule &rule)
{
    Dynamic::Rule::ConstIterator it(rule.constBegin());
    Dynamic::Rule::ConstIterator end(rule.constEnd());
    QMap<QString, QVariant> v;
    QString str;
    #ifdef ENABLE_KDE_SUPPORT
    QString type=i18n("Include");
    #else
    QString type=QObject::tr("Include");
    #endif
    bool exact=true;

    for (int count=0; it!=end; ++it, ++count) {
        if (Dynamic::constExcludeKey==it.key()) {
            if (QLatin1String("true")==it.value()) {
                #ifdef ENABLE_KDE_SUPPORT
                type=i18n("Exclude");
                #else
                type=QObject::tr("Exclude");
                #endif
            }
        } else if (Dynamic::constExactKey==it.key()) {
            if (QLatin1String("false")==it.value()) {
                exact=false;
            }
        } else {
            str+=QString("%1=%2").arg(translateStr(it.key()), it.value());
            if (count<rule.count()-1) {
                str+=' ';
            }
        }
        v.insert(it.key(), it.value());
    }

    if (str.isEmpty()) {
        str=type;
    } else {
        str=type+' '+str;
    }

    if (exact) {
        #ifdef ENABLE_KDE_SUPPORT
        str+=i18n(" (Exact)");
        #else
        str+=QObject::tr(" (Exact)");
        #endif
    }
    i->setText(str);
    i->setData(Qt::UserRole, v);
}

DynamicRulesDialog::DynamicRulesDialog(QWidget *parent)
    #ifdef ENABLE_KDE_SUPPORT
    : KDialog(parent)
    #else
    : QDialog(parent)
    #endif
    , dlg(0)
{
    QWidget *mainWidet = new QWidget(this);
    setupUi(mainWidet);
    #ifdef ENABLE_KDE_SUPPORT
    setMainWidget(mainWidet);
    setButtons(Ok|Cancel);
    setCaption(i18n("Dynamic Rules"));
    enableButton(Ok, false);
    #else
    setWindowTitle(tr("Dynamic Rules"));

    QBoxLayout *layout=new QBoxLayout(QBoxLayout::TopToBottom, this);
    layout->addWidget(mainWidet);
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this);
    layout->addWidget(buttonBox);
    connect(buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(buttonPressed(QAbstractButton *)));
    #endif
    setAttribute(Qt::WA_DeleteOnClose);
    connect(addBtn, SIGNAL(clicked()), SLOT(add()));
    connect(editBtn, SIGNAL(clicked()), SLOT(edit()));
    connect(removeBtn, SIGNAL(clicked()), SLOT(remove()));

    addBtn->setIcon(QIcon::fromTheme("list-add"));
    editBtn->setIcon(QIcon::fromTheme("document-edit"));
    removeBtn->setIcon(QIcon::fromTheme("list-remove"));

    connect(rulesList, SIGNAL(itemSelectionChanged()), SLOT(controlButtons()));
    connect(nameText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    controlButtons();
    resize(540, 400);
}

DynamicRulesDialog::~DynamicRulesDialog()
{
}

void DynamicRulesDialog::edit(const QString &name)
{
    Dynamic::Entry e=Dynamic::self()->entry(name);
    rulesList->clear();
    nameText->setText(name);
    foreach (const Dynamic::Rule &r, e.rules) {
        ::update(new QListWidgetItem(rulesList), r);
    }
    origName=name;
    show();
}

void DynamicRulesDialog::enableOkButton()
{
    bool enable=!nameText->text().trimmed().isEmpty();
    #ifdef ENABLE_KDE_SUPPORT
    enableButton(Ok, enable);
    #else
    buttonBox->button(QDialogButtonBox::Ok)->setEnabled(enable);
    #endif
}

#ifdef ENABLE_KDE_SUPPORT
void DynamicRulesDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Ok: {
        if (save()) {
            accept();
        }
        break;
    }
    case Cancel:
        reject();
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        KDialog::slotButtonClicked(button);
        break;
    default:
        break;
    }
}
#else
void DynamicRulesDialog::buttonPressed(QAbstractButton *button)
{
    if (button==buttonBox->button(QDialogButtonBox::Ok)) {
        if (save()) {
            accept();
        }
    } else if(button==buttonBox->button(QDialogButtonBox::Cancel)) {
        reject();
    }
}
#endif

void DynamicRulesDialog::controlButtons()
{
    int numSel=rulesList->selectedItems().count();
    removeBtn->setEnabled(numSel>0);
    editBtn->setEnabled(1==numSel);
}

void DynamicRulesDialog::add()
{
    if (!dlg) {
        dlg=new DynamicRuleDialog(this);
    }
    if (dlg->edit(Dynamic::Rule())) {
        ::update(new QListWidgetItem(rulesList), dlg->rule());
    }
}

void DynamicRulesDialog::edit()
{
    QList<QListWidgetItem*> items=rulesList->selectedItems();

    if (1!=items.count()) {
        return;
    }
    if (!dlg) {
        dlg=new DynamicRuleDialog(this);
    }
    Dynamic::Rule rule;
    QMap<QString, QVariant> v=items.at(0)->data(Qt::UserRole).toMap();
    QMap<QString, QVariant>::ConstIterator it(v.constBegin());
    QMap<QString, QVariant>::ConstIterator end(v.constEnd());
    for (; it!=end; ++it) {
        rule.insert(it.key(), it.value().toString());
    }
    if (dlg->edit(rule)) {
        ::update(items.at(0), dlg->rule());
    }
}

void DynamicRulesDialog::remove()
{
    QList<QListWidgetItem*> items=rulesList->selectedItems();
    foreach (QListWidgetItem *i, items) {
        delete i;
    }
}

bool DynamicRulesDialog::save()
{
    QString name=nameText->text().trimmed();

    if (name.isEmpty()) {
        return false;
    }

    if (name!=origName && Dynamic::self()->exists(name)) {
        #ifdef ENABLE_KDE_SUPPORT
        if (KMessageBox::No==KMessageBox::warningYesNo(this, i18n("A set of rules named \'%1\' already exists!\nOverwrite?", name),
                                                       i18n("Already Exists"))) {
            return false;
        }
        #else
        if (QMessageBox::No==QMessageBox::warning(this, tr("Already Exists"),
                                                tr("A set of rules named \'%1\' already exists!\nOverwrite?").arg(name),
                                                QMessageBox::Yes|QMessageBox::No, QMessageBox::No)) {
            return false;
        }
        #endif
    }

    Dynamic::Entry entry;
    entry.name=name;
    for (int i=0; i<rulesList->count(); ++i) {
        QListWidgetItem *itm=rulesList->item(i);
        if (itm) {
            QMap<QString, QVariant> v=itm->data(Qt::UserRole).toMap();
            QMap<QString, QVariant>::ConstIterator it(v.constBegin());
            QMap<QString, QVariant>::ConstIterator end(v.constEnd());
            Dynamic::Rule rule;
            for (; it!=end; ++it) {
                rule.insert(it.key(), it.value().toString());
            }
            entry.rules.append(rule);
        }
    }
    bool saved=Dynamic::self()->save(entry);

    if (saved && !origName.isEmpty() && entry.name!=origName) {
        Dynamic::self()->del(origName);
    }
    return saved;
}
