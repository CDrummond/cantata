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

#include "proxystyle.h"
#include "acceleratormanager.h"
#include "gtkstyle.h"
#include "utils.h"
#include "monoicon.h"
#include <QMenu>
#include <QPainter>
#include <QStyleOption>
#include <QApplication>

#if !defined Q_OS_MAC
static const char * constAccelProp="managed-accel";
#endif
const char * ProxyStyle::constModifyFrameProp="mod-frame";

ProxyStyle::ProxyStyle(int modView)
    : modViewFrame(modView)
{
    icons.insert(SP_LineEditClearButton, MonoIcon::icon(FontAwesome::timescircle, QColor(128, 128, 128), QColor(128, 128, 128)));
    icons.insert(SP_MessageBoxCritical, MonoIcon::icon(FontAwesome::timescircleo, MonoIcon::constRed, MonoIcon::constRed));
    icons.insert(SP_MessageBoxWarning, MonoIcon::icon(FontAwesome::exclamationtriangle, QColor(0xff, 0x99, 0x00), QColor(0xff, 0x99, 0x00)));
    icons.insert(SP_MessageBoxQuestion, MonoIcon::icon(FontAwesome::questioncircle, QColor(0x1a, 0x8c, 0xff), QColor(0x1a, 0x8c, 0xff)));
    icons.insert(SP_MessageBoxInformation, MonoIcon::icon(FontAwesome::infocircle, QColor(0x1a, 0x8c, 0xff), QColor(0x1a, 0x8c, 0xff)));

    QColor monoCol = Utils::monoIconColor();
    icons.insert(SP_DialogCancelButton, MonoIcon::icon(FontAwesome::ban, monoCol, monoCol));
    icons.insert(SP_DialogHelpButton, MonoIcon::icon(FontAwesome::lifering, monoCol, monoCol));
    icons.insert(SP_DialogOpenButton, MonoIcon::icon(FontAwesome::foldero, monoCol, monoCol));
    icons.insert(SP_DialogSaveButton, MonoIcon::icon(FontAwesome::save, monoCol, monoCol));
    icons.insert(SP_DialogCloseButton, MonoIcon::icon(FontAwesome::close, MonoIcon::constRed, MonoIcon::constRed));
    icons.insert(SP_DialogApplyButton, MonoIcon::icon(FontAwesome::check, monoCol, monoCol));
    icons.insert(SP_DialogResetButton, MonoIcon::icon(FontAwesome::undo, monoCol, monoCol));
    icons.insert(SP_DialogDiscardButton, MonoIcon::icon(FontAwesome::trash, MonoIcon::constRed, MonoIcon::constRed));
    icons.insert(SP_DialogYesButton, MonoIcon::icon(FontAwesome::check, monoCol, monoCol));
    icons.insert(SP_DialogNoButton, MonoIcon::icon(FontAwesome::times, MonoIcon::constRed, MonoIcon::constRed));
}

void ProxyStyle::polish(QWidget *widget)
{
    #if !defined Q_OS_MAC
    if (widget && qobject_cast<QMenu *>(widget) && !widget->property(constAccelProp).isValid()) {
        AcceleratorManager::manage(widget);
        widget->setProperty(constAccelProp, true);
    }
    #endif
    baseStyle()->polish(widget);
}

int ProxyStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    return SH_DialogButtonBox_ButtonsHaveIcons == hint && Utils::KDE!=Utils::currentDe() ? false : baseStyle()->styleHint(hint, option, widget, returnData);
}

int ProxyStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    return PM_MessageBoxIconSize==metric ? 64 : baseStyle()->pixelMetric(metric, option, widget);
}

void ProxyStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    baseStyle()->drawPrimitive(element, option, painter, widget);
    if (modViewFrame && PE_Frame==element && widget) {
        QVariant v=widget->property(constModifyFrameProp);
        if (v.isValid()) {
            int mod=v.toInt();
            const QRect &r=option->rect;
            if (option->palette.base().color()==Qt::transparent) {
                painter->setPen(QPen(QApplication::palette().color(QPalette::Base), 1));
            } else {
                painter->setPen(QPen(option->palette.base(), 1));
            }
            if (mod&VF_Side && modViewFrame&VF_Side) {
                if (Qt::LeftToRight==option->direction) {
                    painter->drawLine(r.topRight()+QPoint(0, 1), r.bottomRight()+QPoint(0, -1));
                } else {
                    painter->drawLine(r.topLeft()+QPoint(0, 1), r.bottomLeft()+QPoint(0, -1));
                }
            }
            if (mod&VF_Top && modViewFrame&VF_Top) {
                painter->drawLine(r.topLeft()+QPoint(1, 0), r.topRight());
            }
        }
    }
}

QPixmap ProxyStyle::standardPixmap(StandardPixmap sp, const QStyleOption *opt, const QWidget *widget) const
{
    QPixmap pixmap=baseStyle()->standardPixmap(sp, opt, widget);
    const auto icon = icons.find(sp);
    return icon==icons.constEnd() ? pixmap : icon.value().pixmap(pixmap.size());
}

QIcon ProxyStyle::standardIcon(StandardPixmap sp, const QStyleOption *opt, const QWidget *widget) const
{
    const auto icon = icons.find(sp);
    return icon==icons.constEnd() ? baseStyle()->standardIcon(sp, opt, widget) : icon.value();
}
