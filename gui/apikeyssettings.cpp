/*
 * Cantata
 *
 * Copyright (c) 2018-2020 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "apikeyssettings.h"
#include "apikeys.h"
#include "support/utils.h"
#include "widgets/basicitemdelegate.h"
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QItemDelegate>
#include <QDesktopServices>
#include <QLabel>
#include <QUrl>
#include <QPainter>

class ApiKeyDelegate : public BasicItemDelegate
{
public:
    ApiKeyDelegate(QObject *parent)
        : BasicItemDelegate(parent) { }

    QWidget * createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (2==index.column()) {
            return BasicItemDelegate::createEditor(parent, option, index);
        }
        return nullptr;
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.isValid() && 1==index.column()) {
            QStyleOptionViewItem opt=option;
            opt.palette.setColor(QPalette::Text, opt.palette.color(QPalette::Link));
            BasicItemDelegate::paint(painter, opt, index);
            painter->setPen(opt.palette.color(opt.state&QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Link));
            QRect r=opt.rect;
            int pad=3;
            int w = qMin(r.width()-(2*pad), opt.fontMetrics.width(index.data().toString()));
            painter->drawLine(r.x()+pad, r.y()+r.height()-2, r.x()+w-1, r.y()+r.height()-2);
        } else {
            BasicItemDelegate::paint(painter, option, index);
        }
    }
};

ApiKeysSettings::ApiKeysSettings(QWidget *p)
    : QWidget(p)
{
    int spacing=Utils::layoutSpacing(this);
    QVBoxLayout *layout=new QVBoxLayout(this);
    layout->setMargin(0);
    QLabel *label=new QLabel(tr("Cantata uses various internet services to provide covers, radio stream listings, etc. "
                                "Most of these require an application's developer to register with the service, and obtain an API Key. "
                                "Cantata contains the keys required to access these services. Unfortunately, some of these services have "
                                "a usage limit per API key. Therefore, some of Cantata's features may not function if the key (which is shared "
                                "with all Cantata users) has reached its monthly usage limit. To work-around this, you can register with a "
                                "service yourself, and configure Cantata to use your personal API key. This key may be entered below. For any "
                                "blank values below, Cantata will use its default key."), this);
    QFont f(Utils::smallFont(label->font()));
    f.setItalic(true);
    label->setFont(f);
    label->setWordWrap(true);
    layout->addWidget(label);
    layout->addItem(new QSpacerItem(spacing, spacing, QSizePolicy::Fixed, QSizePolicy::Fixed));

    tree=new QTreeWidget(this);
    tree->setHeaderLabels(QStringList() << tr("Service") << tr("Service URL") << tr("API Key"));
    tree->setRootIsDecorated(false);
    tree->setUniformRowHeights(true);
    tree->setItemDelegate(new ApiKeyDelegate(this));
    tree->setSizePolicy(QSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding));
    layout->addWidget(tree);

    int padding = Utils::scaleForDpi(22);
    int nameWidth = padding;
    int urlWidth = padding;
    QFontMetrics fm(tree->font());
    const auto keys = ApiKeys::self()->getDetails();
    for (const auto &k: keys) {
        QTreeWidgetItem *item=new QTreeWidgetItem(tree, QStringList() << k.name << k.url << k.key);
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        nameWidth=qMax(nameWidth, fm.width(k.name)+padding);
        urlWidth=qMax(urlWidth, fm.width(k.url)+padding);
    }
    tree->setColumnWidth(0, nameWidth);
    tree->setColumnWidth(1, urlWidth);
    connect(tree, SIGNAL(itemClicked(QTreeWidgetItem *, int)), SLOT(itemClicked(QTreeWidgetItem *, int)));
}

void ApiKeysSettings::save()
{
    for (int i=0; i<tree->topLevelItemCount(); ++i) {
        ApiKeys::self()->set((ApiKeys::Service)i, tree->topLevelItem(i)->text(2));
    }
    ApiKeys::self()->save();
}

void ApiKeysSettings::itemClicked(QTreeWidgetItem *item, int column)
{
    if (1==column) {
        QDesktopServices::openUrl(QUrl(item->text(column)));
    }
}
