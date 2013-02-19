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

#include "gtkproxystyle.h"
#include "gtkstyle.h"
#include <QComboBox>
#include <QToolBar>
#include <QApplication>
#include <QPainter>

GtkProxyStyle::GtkProxyStyle()
    : QProxyStyle()
{
    slimScrollbars=GtkStyle::themeName().toLower()==QLatin1String("ambiance");
    setBaseStyle(qApp->style());
    toolbarCombo=new QComboBox(new QToolBar());
}

QSize GtkProxyStyle::sizeFromContents(ContentsType type, const QStyleOption *option,  const QSize &size, const QWidget *widget) const
{
    QSize sz=baseStyle()->sizeFromContents(type, option, size, widget);

    if (CT_ComboBox==type && widget && widget->parentWidget() && QString(widget->parentWidget()->metaObject()->className()).endsWith("Page")) {
        QSize orig=baseStyle()->sizeFromContents(type, option, size, widget);
        QSize other=baseStyle()->sizeFromContents(type, option, size, toolbarCombo);
        if (orig.height()>other.height()) {
            return QSize(orig.width(), other.height());
        }
        return orig;
    } else if (slimScrollbars && CT_ScrollBar==type) {
        if (const QStyleOptionSlider *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            int extent(pixelMetric(PM_ScrollBarExtent, option, widget)),
                sliderMin(pixelMetric(PM_ScrollBarSliderMin, option, widget));

            if (sb->orientation == Qt::Horizontal) {
                sz = QSize(sliderMin, extent);
            } else {
                sz = QSize(extent, sliderMin);
            }
        }
    }
    return sz;
}

int GtkProxyStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (slimScrollbars && SH_ScrollView_FrameOnlyAroundContents==hint) {
        return false;
    }

    return baseStyle()->styleHint(hint, option, widget, returnData);
}

int GtkProxyStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    if (slimScrollbars && PM_ScrollBarExtent==metric) {
        return (option ? option->fontMetrics.height() : QApplication::fontMetrics().height())/1.75;
    }
    return baseStyle()->pixelMetric(metric, option, widget);
}

QRect GtkProxyStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    if (slimScrollbars && CC_ScrollBar==control) {
        if (const QStyleOptionSlider *sb = qstyleoption_cast<const QStyleOptionSlider *>(option))  {
            QRect ret;
            bool  horizontal(Qt::Horizontal==sb->orientation);
            int   sbextent(pixelMetric(PM_ScrollBarExtent, sb, widget)),
                  sliderMaxLength(horizontal ? sb->rect.width() : sb->rect.height()),
                  sliderMinLength(pixelMetric(PM_ScrollBarSliderMin, sb, widget)),
                  sliderLength;

            if (sb->maximum != sb->minimum) {
                uint valueRange = sb->maximum - sb->minimum;
                sliderLength = (sb->pageStep * sliderMaxLength) / (valueRange + sb->pageStep);

                if (sliderLength < sliderMinLength) {
                    sliderLength = sliderMinLength;
                }
                if (sliderLength > sliderMaxLength) {
                    sliderLength = sliderMaxLength;
                }
            } else {
                sliderLength = sliderMaxLength;
            }

            int sliderstart(sliderPositionFromValue(sb->minimum, sb->maximum, sb->sliderPosition, sliderMaxLength - sliderLength, sb->upsideDown));

            // Subcontrols
            switch(subControl)
            {
            case SC_ScrollBarSubLine:
            case SC_ScrollBarAddLine:
                return QRect();
            case SC_ScrollBarSubPage:
                if (horizontal) {
                    ret.setRect(0, 0, sliderstart, sbextent);
                } else {
                    ret.setRect(0, 0, sbextent, sliderstart);
                }
                break;
            case SC_ScrollBarAddPage:
                if (horizontal) {
                    ret.setRect(sliderstart + sliderLength, 0, sliderMaxLength - sliderstart - sliderLength, sbextent);
                } else {
                    ret.setRect(0, sliderstart + sliderLength, sbextent, sliderMaxLength - sliderstart - sliderLength);
                }
                break;
            case SC_ScrollBarGroove:
                if (horizontal) {
                    ret=QRect(0, 0, sb->rect.width(), sb->rect.height());
                } else {
                    ret=QRect(0, 0, sb->rect.width(), sb->rect.height());
                }
                break;
            case SC_ScrollBarSlider:
                if (horizontal) {
                    ret=QRect(sliderstart, 0, sliderLength, sbextent);
                } else {
                    ret=QRect(0, sliderstart, sbextent, sliderLength);
                }
                break;
            default:
                ret = baseStyle()->subControlRect(control, option, subControl, widget);
                break;
            }
            return visualRect(sb->direction/*Qt::LeftToRight*/, sb->rect, ret);
        }
    }
    return baseStyle()->subControlRect(control, option, subControl, widget);
}

static QPainterPath buildPath(const QRectF &r, double radius)
{
    QPainterPath path;
    double diameter(radius*2);

    path.moveTo(r.x()+r.width(), r.y()+r.height()-radius);
    path.arcTo(r.x()+r.width()-diameter, r.y(), diameter, diameter, 0, 90);
    path.arcTo(r.x(), r.y(), diameter, diameter, 90, 90);
    path.arcTo(r.x(), r.y()+r.height()-diameter, diameter, diameter, 180, 90);
    path.arcTo(r.x()+r.width()-diameter, r.y()+r.height()-diameter, diameter, diameter, 270, 90);
    return path;
}

void GtkProxyStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    if (slimScrollbars && CC_ScrollBar==control) {
        if (const QStyleOptionSlider *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            QRect r=option->rect;
            QRect slider=subControlRect(control, option, SC_ScrollBarSlider, widget);
            painter->save();
            painter->fillRect(r, option->palette.base());
            QColor col=option->palette.foreground().color();
            col.setAlphaF(0.15);
            painter->setPen(col);
            if (Qt::Horizontal==sb->orientation) {
                painter->drawLine(r.x(), r.y(), r.x()+r.width()-1, r.y());
            } else if (Qt::RightToLeft==QApplication::layoutDirection()) {
                painter->drawLine(r.x()+r.width()-1, r.y(), r.x()+r.width()-1, r.y()+r.height()-1);
            } else {
                painter->drawLine(r.x(), r.y(), r.x(), r.y()+r.height()-1);
            }
            if (slider.isValid()) {
                painter->setRenderHint(QPainter::Antialiasing, true);
                if (Qt::Horizontal==sb->orientation) {
                    slider.adjust(0, 2, 0, -2);
                } else {
                    slider.adjust(2, 0, -2, 0);
                }
                QPainterPath path=buildPath(QRectF(slider.x()+0.5, slider.y()+0.5, slider.width()-1, slider.height()-1),
                                            (Qt::Horizontal==sb->orientation ? r.height() : r.width())/4);
                QColor col(option->palette.highlight().color());
                if (option->state&State_Active && !(option->state&State_MouseOver)) {
                    col.setAlphaF(0.75);
                }
                painter->fillPath(path, col);
                if (option->state&State_Active && !(option->state&State_MouseOver)) {
                    col.setAlphaF(0.25);
                }
                painter->setPen(col);
                painter->drawPath(path);
            }
            painter->restore();
            return;
        }
    }
    baseStyle()->drawComplexControl(control, option, painter, widget);
}
