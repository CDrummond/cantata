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
#include <QtGui/QStandardItem>
#include <QtGui/QStandardItemModel>
#include <QtGui/QSortFilterProxyModel>
#include <QtGui/QWhatsThis>

class RulesSort : public QSortFilterProxyModel
{
public:
    RulesSort(QObject *parent)
        : QSortFilterProxyModel(parent) {
        setDynamicSortFilter(true);
        setFilterCaseSensitivity(Qt::CaseInsensitive);
        setSortCaseSensitivity(Qt::CaseInsensitive);
        setSortLocaleAware(true);
        sort(0);
    }

    virtual ~RulesSort() {
    }

    bool lessThan(const QModelIndex &left, const QModelIndex &right) const {
        bool lInc=left.data(Qt::UserRole+2).toBool();
        bool rInc=right.data(Qt::UserRole+2).toBool();

        if (lInc==rInc) {
            return left.data(Qt::DisplayRole).toString().localeAwareCompare(right.data(Qt::DisplayRole).toString())<0;
        } else {
            return lInc ? true : false;
        }
    }
};

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

static void update(QStandardItem *i, const Dynamic::Rule &rule)
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
    bool include=true;

    for (int count=0; it!=end; ++it, ++count) {
        if (Dynamic::constExcludeKey==it.key()) {
            if (QLatin1String("true")==it.value()) {
                #ifdef ENABLE_KDE_SUPPORT
                type=i18n("Exclude");
                #else
                type=QObject::tr("Exclude");
                #endif
                include=false;
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
    i->setData(v);
    i->setData(include, Qt::UserRole+2);
    i->setFlags(Qt::ItemIsSelectable| Qt::ItemIsEnabled);
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

    connect(rulesList, SIGNAL(itemsSelected(bool)), SLOT(controlButtons()));
    connect(nameText, SIGNAL(textChanged(const QString &)), SLOT(enableOkButton()));
    connect(aboutLabel, SIGNAL(leftClickedUrl()), this, SLOT(showAbout()));

    model=new QStandardItemModel(this);
    proxy=new RulesSort(this);
    proxy->setSourceModel(model);
    rulesList->setModel(proxy);

    controlButtons();
    resize(500, 240);
}

DynamicRulesDialog::~DynamicRulesDialog()
{
}

void DynamicRulesDialog::edit(const QString &name)
{
    Dynamic::Entry e=Dynamic::self()->entry(name);
    if (model->rowCount()) {
        model->removeRows(0, model->rowCount());
    }
    nameText->setText(name);
    foreach (const Dynamic::Rule &r, e.rules) {
        QStandardItem *item = new QStandardItem();
        ::update(item, r);
        model->setItem(model->rowCount(), 0, item);
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
    int numSel=rulesList->selectedIndexes().count();
    removeBtn->setEnabled(numSel>0);
    editBtn->setEnabled(1==numSel);
}

void DynamicRulesDialog::add()
{
    if (!dlg) {
        dlg=new DynamicRuleDialog(this);
    }
    if (dlg->edit(Dynamic::Rule())) {
        QStandardItem *item = new QStandardItem();
        ::update(item, dlg->rule());
        model->setItem(model->rowCount(), 0, item);
    }
}

void DynamicRulesDialog::edit()
{
    QModelIndexList items=rulesList->selectedIndexes();

    if (1!=items.count()) {
        return;
    }
    if (!dlg) {
        dlg=new DynamicRuleDialog(this);
    }
    QStandardItem *item=model->itemFromIndex(proxy->mapToSource(items.at(0)));
    Dynamic::Rule rule;
    QMap<QString, QVariant> v=item->data().toMap();
    QMap<QString, QVariant>::ConstIterator it(v.constBegin());
    QMap<QString, QVariant>::ConstIterator end(v.constEnd());
    for (; it!=end; ++it) {
        rule.insert(it.key(), it.value().toString());
    }
    if (dlg->edit(rule)) {
        ::update(item, dlg->rule());
    }
}

void DynamicRulesDialog::remove()
{
    QModelIndexList items=rulesList->selectedIndexes();
    QList<int> rows;
    foreach (const QModelIndex &i, items) {
        rows.append(proxy->mapToSource(i).row());
    }
    qSort(rows);
    for (int r=rows.count()-1; r<=0; --r) {
        model->removeRow(r);
    }
}

void DynamicRulesDialog::showAbout()
{
    QWhatsThis::showText(aboutLabel->mapToGlobal(aboutLabel->geometry().topLeft()),
                         #ifdef ENABLE_KDE_SUPPORT
                         i18n("<p>Cantata will query your library using all of the rules listed. "
                              "The list of <i>Include</i> rules will be used to build a set of songs that can be used. "
                              "The list of <i>Exclude</i> rules will be used to build a set of songs that cannot be used. "
                              "If there are no <i>Include</i> rules, Cantata will assume that all songs (bar those from <i>Exlude</i>) can be used. <br/>"
                              "e.g. to have Cantata look for 'Rock songs by Wibble OR songs by Various Artists', you would need the following: "
                              "<ul><li>Include AlbumArtist=Wibble Genre=Rock</li><li>Include AlbumArtist=Various Artists</li></ul> "
                              "to have Cantata look for 'Songs by Wibble but not from album Abc', you would need the following: "
                              "<ul><li>Include AlbumArtist=Wibble</li><li>Exclude Album=Avc</li></ul>"
                              "After the set of usable songs has been created, Cantata will randomly select songs to "
                              "keep the play queue filled with 10 entries.</p>")
                        #else
                        tr("<p>Cantata will query your library using all of the rules listed. "
                           "The list of <i>Include</i> rules will be used to build a set of songs that can be used. "
                           "The list of <i>Exclude</i> rules will be used to build a set of songs that cannot be used. "
                           "If there are no <i>Include</i> rules, Cantata will assume that all songs (bar those from <i>Exlude</i>) can be used. <br/>"
                           "e.g. to have Cantata look for 'Rock songs by Wibble OR songs by Various Artists', you would need the following: "
                           "<ul><li>Include AlbumArtist=Wibble Genre=Rock</li><li>Include AlbumArtist=Various Artists</li></ul> "
                           "to have Cantata look for 'Songs by Wibble but not from album Abc', you would need the following: "
                           "<ul><li>Include AlbumArtist=Wibble</li><li>Exclude Album=Avc</li></ul>"
                           "After the set of usable songs has been created, Cantata will randomly select songs to "
                           "keep the play queue filled with 10 entries.</p>")
                        #endif
                        );

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
    for (int i=0; i<model->rowCount(); ++i) {
        QStandardItem *itm=model->item(i);
        if (itm) {
            QMap<QString, QVariant> v=itm->data().toMap();
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
