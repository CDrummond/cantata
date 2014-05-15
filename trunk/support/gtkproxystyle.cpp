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

#include "gtkproxystyle.h"
#include "gtkstyle.h"
#include "shortcuthandler.h"
#ifndef ENABLE_KDE_SUPPORT
#include "acceleratormanager.h"
#endif
#include <QComboBox>
#include <QSpinBox>
#include <QScrollBar>
#include <QAbstractScrollArea>
#include <QAbstractItemView>
#include <QMenu>
#include <QToolBar>
#include <QApplication>
#include <QPainter>

static const char * constOnCombo="on-combo";
#ifndef ENABLE_KDE_SUPPORT
static const char * constAccelProp="catata-accel";
#endif

static bool isOnCombo(const QWidget *w)
{
    return w && (qobject_cast<const QComboBox *>(w) || isOnCombo(w->parentWidget()));
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

static inline void addEventFilter(QObject *object, QObject *filter)
{
    object->removeEventFilter(filter);
    object->installEventFilter(filter);
}

GtkProxyStyle::GtkProxyStyle(ScrollbarType sb, bool styleSpin, const QMap<QString, QString> &c, bool modView)
    : TouchProxyStyle(styleSpin)
    , sbarType(sb)
    , modViewFrame(modView)
    , css(c)
{
    shortcutHander=new ShortcutHandler(this);

    if (SB_Standard!=sbarType) {
        QByteArray env=qgetenv("LIBOVERLAY_SCROLLBAR");
        if (!env.isEmpty() && env!="1") {
            sbarType=SB_Standard;
        }
    }

    setBaseStyle(qApp->style());
    if (SB_Standard!=sbarType) {
        int fh=QApplication::fontMetrics().height();
        sbarPlainViewWidth=fh/1.75;
    }
}

GtkProxyStyle::~GtkProxyStyle()
{
}

QSize GtkProxyStyle::sizeFromContents(ContentsType type, const QStyleOption *option,  const QSize &size, const QWidget *widget) const
{
    QSize sz=TouchProxyStyle::sizeFromContents(type, option, size, widget);

    if (SB_Standard!=sbarType && CT_ScrollBar==type) {
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
    switch (hint) {
    case SH_DialogButtonBox_ButtonsHaveIcons:
        return false;
    case SH_UnderlineShortcut:
        return widget ? shortcutHander->showShortcut(widget) : true;
    case SH_ScrollView_FrameOnlyAroundContents:
        if (SB_Standard!=sbarType) {
            return false;
        }
    default:
        break;
    }

    return baseStyle()->styleHint(hint, option, widget, returnData);
}

int GtkProxyStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    if (SB_Standard!=sbarType && PM_ScrollBarExtent==metric) {
        return sbarPlainViewWidth;
    }
    return baseStyle()->pixelMetric(metric, option, widget);
}

QRect GtkProxyStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    if (SB_Standard!=sbarType && CC_ScrollBar==control) {
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
                ret=QRect(0, 0, sb->rect.width(), sb->rect.height());
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
    return TouchProxyStyle::subControlRect(control, option, subControl, widget);
}

void GtkProxyStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    if (SB_Standard!=sbarType && CC_ScrollBar==control) {
        if (const QStyleOptionSlider *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            QRect r=option->rect;
            QRect slider=subControlRect(control, option, SC_ScrollBarSlider, widget);
            painter->save();
            if (widget && widget->property(constOnCombo).toBool()) {
                painter->fillRect(r, QApplication::palette().color(QPalette::Background)); // option->palette.background());
            } else if (!widget || widget->testAttribute(Qt::WA_OpaquePaintEvent)) {
                if (option->palette.base().color()==Qt::transparent) {
                    painter->fillRect(r, QApplication::palette().color(QPalette::Base));
                } else {
                    painter->fillRect(r, option->palette.base());
                }
            }

            if (slider.isValid()) {
                bool inactive=!(sb->activeSubControls&SC_ScrollBarSlider && (option->state&State_MouseOver || option->state&State_Sunken));
                int adjust=inactive ? 3 : 1;
                painter->setRenderHint(QPainter::Antialiasing, true);
                if (Qt::Horizontal==sb->orientation) {
                    slider.adjust(1, adjust, -1, -adjust);
                } else {
                    slider.adjust(adjust, 1, -adjust, -1);
                }
                int dimension=(Qt::Horizontal==sb->orientation ? slider.height() : slider.width());
                QPainterPath path=buildPath(QRectF(slider.x()+0.5, slider.y()+0.5, slider.width()-1, slider.height()-1),
                                            dimension>6 ? (dimension/4.0) : (dimension/8.0));
                QColor col(option->palette.highlight().color());
                if (!(option->state&State_Active)) {
                    col=col.darker(115);
                }
                painter->fillPath(path, col);
                painter->setPen(col);
                painter->drawPath(path);
            }

            painter->restore();
            return;
        }
    }
    TouchProxyStyle::drawComplexControl(control, option, painter, widget);
}

void GtkProxyStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (PE_PanelScrollAreaCorner==element && SB_Thin==sbarType && option) {
        painter->fillRect(option->rect, option->palette.brush(QPalette::Base));
    } else {
        baseStyle()->drawPrimitive(element, option, painter, widget);
        if (modViewFrame && PE_Frame==element && widget && widget->property(GtkStyle::constHideFrameProp).isValid()) {
            const QRect &r=option->rect;
            if (option->palette.base().color()==Qt::transparent) {
                painter->setPen(QPen(QApplication::palette().color(QPalette::Base), 1));
            } else {
                painter->setPen(QPen(option->palette.base(), 1));
            }
            if (Qt::LeftToRight==option->direction) {
                painter->drawLine(r.topRight()+QPoint(0, 1), r.bottomRight()+QPoint(0, -1));
            } else {
                painter->drawLine(r.topLeft()+QPoint(0, 1), r.bottomLeft()+QPoint(0, -1));
            }
        }
    }
}

void GtkProxyStyle::polish(QWidget *widget)
{
    if (SB_Standard!=sbarType) {
        if (qobject_cast<QScrollBar *>(widget)) {
            if (isOnCombo(widget)) {
                widget->setProperty(constOnCombo, true);
            }
        } else if (qobject_cast<QAbstractScrollArea *>(widget) && widget->inherits("QComboBoxListView")) {
            QAbstractScrollArea *sa=static_cast<QAbstractScrollArea *>(widget);
            QWidget *sb=sa->horizontalScrollBar();
            if (sb) {
                sb->setProperty(constOnCombo, true);
            }
            sb=sa->verticalScrollBar();
            if (sb) {
                sb->setProperty(constOnCombo, true);
            }
        }
    }

    #ifndef ENABLE_KDE_SUPPORT
    if (widget && qobject_cast<QMenu *>(widget) && !widget->property(constAccelProp).isValid()) {
        AcceleratorManager::manage(widget);
        widget->setProperty(constAccelProp, true);
    }
    #endif

    // Apply CSS only to particular widgets. With Qt5.2 if we apply CSS to whole application, then QStyleSheetStyle does
    // NOT call sizeFromContents for spinboxes :-(
    if (widget->styleSheet().isEmpty()) {
        QMap<QString, QString>::ConstIterator it=css.end();
        if (qobject_cast<QToolBar *>(widget)) {
            it=css.find(QLatin1String(widget->metaObject()->className())+QLatin1Char('#')+widget->objectName());
        } else if (qobject_cast<QMenu *>(widget)) {
            it=css.find(QLatin1String(widget->metaObject()->className()));
        }
        if (css.end()!=it) {
            widget->setStyleSheet(it.value());
        }
    }
    TouchProxyStyle::polish(widget);
}

void GtkProxyStyle::polish(QPalette &pal)
{
    baseStyle()->polish(pal);
}

void GtkProxyStyle::polish(QApplication *app)
{
    addEventFilter(app, shortcutHander);
    baseStyle()->polish(app);
}

void GtkProxyStyle::unpolish(QWidget *widget)
{
    baseStyle()->unpolish(widget);
}

void GtkProxyStyle::unpolish(QApplication *app)
{
    app->removeEventFilter(shortcutHander);
    baseStyle()->unpolish(app);
}
