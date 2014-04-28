/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "combobox.h"
#include "lineedit.h"
#include "utils.h"
#include <QAbstractItemView>
#include <QStyle>
#include <QStyleOption>
#include <QStyledItemDelegate>
#include <QDesktopWidget>
#include <QApplication>
#include <QPainter>
#include "gtkstyle.h"
#include "flickcharm.h"

// Max number of items before we try to force a scrollbar in popup menu...
static int maxPopupItemCount=-1;

class ComboItemDelegate : public QStyledItemDelegate
{
public:
    ComboItemDelegate(ComboBox *p) : QStyledItemDelegate(p), mCombo(p) { }
    virtual ~ComboItemDelegate() { }
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QSize sz=QStyledItemDelegate::sizeHint(option, index);
        int minH=option.fontMetrics.height()*2;
        if (sz.height()<minH) {
            sz.setHeight(minH);
        }
        return sz;
    }
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (GtkStyle::isActive()) {
            QStyleOptionMenuItem opt = getStyleOption(option, index);
            painter->fillRect(option.rect, opt.palette.background());
            mCombo->style()->drawControl(QStyle::CE_MenuItem, &opt, painter, mCombo);
        } else {
            QStyledItemDelegate::paint(painter, option, index);
        }
    }

    static bool isSeparator(const QModelIndex &index)
    {
        return index.data(Qt::AccessibleDescriptionRole).toString() == QLatin1String("separator");
    }

    QStyleOptionMenuItem getStyleOption(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QStyleOptionMenuItem menuOption;

        QPalette resolvedpalette = option.palette.resolve(QApplication::palette("QMenu"));
        QVariant value = index.data(Qt::ForegroundRole);
        if (value.canConvert<QBrush>()) {
            resolvedpalette.setBrush(QPalette::WindowText, qvariant_cast<QBrush>(value));
            resolvedpalette.setBrush(QPalette::ButtonText, qvariant_cast<QBrush>(value));
            resolvedpalette.setBrush(QPalette::Text, qvariant_cast<QBrush>(value));
        }
        menuOption.palette = resolvedpalette;
        menuOption.state = QStyle::State_None;
        if (mCombo->window()->isActiveWindow()) {
            menuOption.state = QStyle::State_Active;
        }
        if ((option.state & QStyle::State_Enabled) && (index.model()->flags(index) & Qt::ItemIsEnabled)) {
            menuOption.state |= QStyle::State_Enabled;
        } else {
            menuOption.palette.setCurrentColorGroup(QPalette::Disabled);
        }
        if (option.state & QStyle::State_Selected) {
            menuOption.state |= QStyle::State_Selected;
        }
        menuOption.checkType = QStyleOptionMenuItem::NonExclusive;
        menuOption.checked = mCombo->currentIndex() == index.row();
        if (isSeparator(index)) {
            menuOption.menuItemType = QStyleOptionMenuItem::Separator;
        } else {
            menuOption.menuItemType = QStyleOptionMenuItem::Normal;
        }

        QVariant variant = index.model()->data(index, Qt::DecorationRole);
        switch (variant.type()) {
        case QVariant::Icon:
            menuOption.icon = qvariant_cast<QIcon>(variant);
            break;
        case QVariant::Color: {
            static QPixmap pixmap(option.decorationSize);
            pixmap.fill(qvariant_cast<QColor>(variant));
            menuOption.icon = pixmap;
            break;
        }
        default:
            menuOption.icon = qvariant_cast<QPixmap>(variant);
            break;
        }
        if (index.data(Qt::BackgroundRole).canConvert<QBrush>()) {
            menuOption.palette.setBrush(QPalette::All, QPalette::Background,
                                        qvariant_cast<QBrush>(index.data(Qt::BackgroundRole)));
        }
        menuOption.text = index.model()->data(index, Qt::DisplayRole).toString()
                               .replace(QLatin1Char('&'), QLatin1String("&&"));
        menuOption.tabWidth = 0;
        menuOption.maxIconWidth =  option.decorationSize.width() + 4;
        menuOption.menuRect = option.rect;
        menuOption.rect = option.rect;
        menuOption.font = mCombo->font();
        menuOption.fontMetrics = QFontMetrics(menuOption.font);
        return menuOption;
    }
    ComboBox *mCombo;
};

ComboBox::ComboBox(QWidget *p)
    #ifdef ENABLE_KDE_SUPPORT
    : KComboBox(p)
    #else
    : QComboBox(p)
    #endif
    , toggleState(false)
{
    if (-1==maxPopupItemCount) {
        if (QApplication::desktop()) {
            maxPopupItemCount=((QApplication::desktop()->height()/(QApplication::fontMetrics().height()*1.5))*0.75)+0.5;
        } else {
            maxPopupItemCount=32;
        }
    }
    if (Utils::touchFriendly()) {
        setItemDelegate(new ComboItemDelegate(this));
        FlickCharm::self()->activateOn(view());
    }
}

void ComboBox::setEditable(bool editable)
{
    QComboBox::setEditable(editable);
    if (editable) {
        LineEdit *edit = new LineEdit(this);
        setLineEdit(edit);
    }
}

void ComboBox::showPopup()
{
    if (GtkStyle::isActive() && count()>(maxPopupItemCount-2)) {
        toggleState=!isEditable();

        // Hacky, but if we set the combobox as editable - the style gives the
        // popup a scrollbar. This is more convenient if we have lots of items!
        if (toggleState) {
            setMaxVisibleItems(maxPopupItemCount);
            QComboBox::setEditable(true);
            lineEdit()->setReadOnly(true);
        }
    }
    QComboBox::showPopup();
    if (Utils::touchFriendly()) {
        view()->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view()->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    }

    if (GtkStyle::isActive() && parentWidget() && view()->parentWidget() && count()>maxPopupItemCount) {
        // Also, if the size of the popup is more than required for 32 items, then
        // restrict its height...
        int maxHeight=maxPopupItemCount*view()->sizeHintForRow(0);
        QRect geo(view()->parentWidget()->geometry());
        QRect r(view()->parentWidget()->rect());
        if (geo.height()>maxHeight) {
            geo=QRect(geo.x(), geo.y()+(geo.height()-maxHeight), geo.width(), maxHeight);
            r=QRect(r.x(), r.y()+(r.height()-maxHeight), r.width(), maxHeight);
        }
        QPoint popupBot=view()->parentWidget()->mapToGlobal(r.bottomLeft());
        QPoint bot=mapToGlobal(rect().bottomLeft());

        if (popupBot.y()<bot.y()) {
            geo=QRect(geo.x(), geo.y()+(bot.y()-popupBot.y()), geo.width(), geo.height());
        } else {
            QPoint popupTop=view()->parentWidget()->mapToGlobal(r.topLeft());
            QPoint top=mapToGlobal(rect().topLeft());
            if (popupTop.y()>top.y()) {
                geo=QRect(geo.x(), geo.y()-(popupTop.y()-top.y()), geo.width(), geo.height());
            }
        }
        view()->parentWidget()->setGeometry(geo);

        #ifndef ENABLE_KDE_SUPPORT
        // Hide scrollers - these look ugly...
        foreach (QObject *c, view()->parentWidget()->children()) {
            if (0==qstrcmp("QComboBoxPrivateScroller", c->metaObject()->className())) {
                static_cast<QWidget *>(c)->setMaximumHeight(0);
            }
        }
        #endif
    }
}

void ComboBox::hidePopup()
{
    if (GtkStyle::isActive() && count()>(maxPopupItemCount-2) && toggleState) {
        QComboBox::setEditable(false);
    }
    QComboBox::hidePopup();
}
