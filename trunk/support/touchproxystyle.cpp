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

#include "touchproxystyle.h"
#include "utils.h"
#include "gtkstyle.h"
#include "flickcharm.h"
#include <QSpinBox>
#include <QComboBox>
#include <QPainter>
#include <QStyleOptionSpinBox>
#include <QAbstractScrollArea>
#include <QStyledItemDelegate>
#include <QApplication>

static void drawSpinButton(QPainter *painter, const QRect &r, const QColor &col, bool isPlus)
{
    int length=r.height()*0.5;
    int lineWidth=length<24 ? 2 : 4;
    if (length<(lineWidth*2)) {
        length=lineWidth*2;
    } else if (length%2) {
        length++;
    } 

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->fillRect(r.x()+((r.width()-length)/2), r.y()+((r.height()-lineWidth)/2), length, lineWidth, col);
    if (isPlus) {
        painter->fillRect(r.x()+((r.width()-lineWidth)/2), r.y()+((r.height()-length)/2), lineWidth, length, col);
    }
    painter->restore();
}

class ComboItemDelegate : public QStyledItemDelegate
{
public:
    ComboItemDelegate(QComboBox *p) : QStyledItemDelegate(p), combo(p) { }
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
            combo->style()->drawControl(QStyle::CE_MenuItem, &opt, painter, combo);
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
        if (combo->window()->isActiveWindow()) {
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
        menuOption.checked = combo->currentIndex() == index.row();
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
        menuOption.font = combo->font();
        menuOption.fontMetrics = QFontMetrics(menuOption.font);
        return menuOption;
    }
    QComboBox *combo;
};

TouchProxyStyle::TouchProxyStyle(bool touchSpin)
    : ProxyStyle()
    , touchStyleSpin(touchSpin)
{
    spinButtonRatio=touchSpin && Utils::touchFriendly() ? 1.5 : 1.25;
}

TouchProxyStyle::~TouchProxyStyle()
{
}

QSize TouchProxyStyle::sizeFromContents(ContentsType type, const QStyleOption *option,  const QSize &size, const QWidget *widget) const
{
    QSize sz=baseStyle()->sizeFromContents(type, option, size, widget);

    if (touchStyleSpin && CT_SpinBox==type) {
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            if (QAbstractSpinBox::NoButtons!=spinBox->buttonSymbols) {
                #if QT_VERSION < 0x050200
                sz += QSize(0, 1);
                #endif
                #if QT_VERSION >= 0x050000
                // Qt5 does not seem to be taking special value, or suffix, into account when calculatng width...
                if (widget && qobject_cast<const QSpinBox *>(widget)) {
                    const QSpinBox *spin=static_cast<const QSpinBox *>(widget);
                    QString special=spin->specialValueText();
                    int minWidth=0;
                    if (!special.isEmpty()) {
                        minWidth=option->fontMetrics.width(special+QLatin1String(" "));
                    }

                    QString suffix=spin->suffix()+QLatin1String(" ");
                    minWidth=qMax(option->fontMetrics.width(QString::number(spin->minimum())+suffix), minWidth);
                    minWidth=qMax(option->fontMetrics.width(QString::number(spin->maximum())+suffix), minWidth);

                    if (minWidth>0) {
                        int frameWidth=baseStyle()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, 0);
                        int buttonWidth=(sz.height()-(frameWidth*2))*spinButtonRatio;
                        minWidth=((minWidth+(buttonWidth+frameWidth)*2)*1.05)+0.5;
                        if (sz.width()<minWidth) {
                            sz.setWidth(minWidth);
                        }
                    }
                } else
                #endif
                if (!GtkStyle::isActive()) {
                    sz.setWidth(sz.width()+6);
                }
            }
        }
    }
    return sz;
}

QRect TouchProxyStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    if (touchStyleSpin && CC_SpinBox==control) {
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            if (QAbstractSpinBox::NoButtons!=spinBox->buttonSymbols) {
                int border=2;
                int padBeforeButtons=GtkStyle::isActive() ? 0 : 2;
                int internalHeight=spinBox->rect.height()-(border*2);
                int internalWidth=internalHeight*spinButtonRatio;
                switch (subControl) {
                case SC_SpinBoxUp:
                    return Qt::LeftToRight==spinBox->direction
                                ? QRect(spinBox->rect.width()-(internalWidth+border), border, internalWidth, internalHeight)
                                : QRect(border, border, internalWidth, internalHeight);
                case SC_SpinBoxDown:
                    return Qt::LeftToRight==spinBox->direction
                                ? QRect(spinBox->rect.width()-((internalWidth*2)+border), border, internalWidth, internalHeight)
                                : QRect(internalWidth+border, border, internalWidth, internalHeight);
                case SC_SpinBoxEditField:
                    return Qt::LeftToRight==spinBox->direction
                            ? QRect(border, border, spinBox->rect.width()-((internalWidth*2)+border+padBeforeButtons), internalHeight)
                            : QRect(((internalWidth*2)+border), border, spinBox->rect.width()-((internalWidth*2)+border+padBeforeButtons), internalHeight);
                case SC_SpinBoxFrame:
                    return spinBox->rect;
                default:
                    break;
                }
            }
        }
    }
    return baseStyle()->subControlRect(control, option, subControl, widget);
}

void TouchProxyStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    if (touchStyleSpin && CC_SpinBox==control) {
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            if (QAbstractSpinBox::NoButtons!=spinBox->buttonSymbols) {
                QStyleOptionFrame opt;
                opt.state=spinBox->state;
                opt.state|=State_Sunken;
                opt.rect=spinBox->rect;
                opt.palette=spinBox->palette;
                opt.lineWidth=baseStyle()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, widget);
                opt.midLineWidth=0;
                opt.fontMetrics=spinBox->fontMetrics;
                opt.direction=spinBox->direction;
                opt.type=QStyleOption::SO_Frame;
                opt.version=1;
                baseStyle()->drawPrimitive(PE_PanelLineEdit, &opt, painter, 0);

                QRect plusRect=subControlRect(CC_SpinBox, spinBox, SC_SpinBoxUp, widget);
                QRect minusRect=subControlRect(CC_SpinBox, spinBox, SC_SpinBoxDown, widget);
                QColor separatorColor(spinBox->palette.foreground().color());
                separatorColor.setAlphaF(0.15);
                painter->setPen(separatorColor);
                if (Qt::LeftToRight==spinBox->direction) {
                    painter->drawLine(plusRect.topLeft(), plusRect.bottomLeft());
                    painter->drawLine(minusRect.topLeft(), minusRect.bottomLeft());
                } else {
                    painter->drawLine(plusRect.topRight(), plusRect.bottomRight());
                    painter->drawLine(minusRect.topRight(), minusRect.bottomRight());
                }

                if (option->state&State_Sunken) {
                    QRect fillRect;

                    if (spinBox->activeSubControls&SC_SpinBoxUp) {
                        fillRect=plusRect;
                    } else if (spinBox->activeSubControls&SC_SpinBoxDown) {
                        fillRect=minusRect;
                    }
                    if (!fillRect.isEmpty()) {
                        QColor col=spinBox->palette.highlight().color();
                        col.setAlphaF(0.1);
                        painter->fillRect(fillRect.adjusted(1, 1, -1, -1), col);
                    }
                }

                drawSpinButton(painter, plusRect,
                               spinBox->palette.color(spinBox->state&State_Enabled && (spinBox->stepEnabled&QAbstractSpinBox::StepUpEnabled)
                                ? QPalette::Current : QPalette::Disabled, QPalette::Text), true);
                drawSpinButton(painter, minusRect,
                               spinBox->palette.color(spinBox->state&State_Enabled && (spinBox->stepEnabled&QAbstractSpinBox::StepDownEnabled)
                                ? QPalette::Current : QPalette::Disabled, QPalette::Text), false);
                return;
            }
        }
    }
    baseStyle()->drawComplexControl(control, option, painter, widget);
}

void TouchProxyStyle::polish(QWidget *widget)
{
    if (Utils::touchFriendly()) {
        if (qobject_cast<QAbstractScrollArea *>(widget)) {
            FlickCharm::self()->activateOn(widget);
        } else if (qobject_cast<QComboBox *>(widget)) {
            QComboBox *combo=static_cast<QComboBox *>(widget);
            combo->setItemDelegate(new ComboItemDelegate(combo));
        }
    }
    ProxyStyle::polish(widget);
}
