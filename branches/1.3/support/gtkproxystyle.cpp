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
#include <QMenu>
#include <QToolBar>
#include <QApplication>
#include <QPainter>

static const char * constOnCombo="on-combo";
#ifndef ENABLE_KDE_SUPPORT
static const char * constAccelProp="catata-accel";
#endif
static const double constSpinButtonRatio=1.25;

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

static inline void addEventFilter(QObject *object, QObject *filter)
{
    object->removeEventFilter(filter);
    object->installEventFilter(filter);
}

GtkProxyStyle::GtkProxyStyle(ScrollbarType sb, bool styleSpin, const QMap<QString, QString> &c)
    : QProxyStyle()
    , css(c)
{
    shortcutHander=new ShortcutHandler(this);

    sbarType=sb;
    touchStyleSpin=styleSpin;

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
    QSize sz=baseStyle()->sizeFromContents(type, option, size, widget);

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
    } else if (touchStyleSpin && CT_SpinBox==type) {
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
                        int buttonWidth=(sz.height()-(frameWidth*2))*constSpinButtonRatio;
                        minWidth=((minWidth+(buttonWidth+frameWidth)*2)*1.05)+0.5;
                        if (sz.width()<minWidth) {
                            sz.setWidth(minWidth);
                        }
                    }
                }
                #endif
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
    } else if (touchStyleSpin && CC_SpinBox==control) {
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            if (QAbstractSpinBox::NoButtons!=spinBox->buttonSymbols) {
                int border=2;
                int internalHeight=spinBox->rect.height()-(border*2);
                int internalWidth=internalHeight*constSpinButtonRatio;
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
                            ? QRect(border, border, spinBox->rect.width()-((internalWidth*2)+border), internalHeight)
                            : QRect(((internalWidth*2)+border), border, spinBox->rect.width()-((internalWidth*2)+border), internalHeight);
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

void GtkProxyStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    if (SB_Standard!=sbarType && CC_ScrollBar==control) {
        if (const QStyleOptionSlider *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            QRect r=option->rect;
            QRect slider=subControlRect(control, option, SC_ScrollBarSlider, widget);
            painter->save();

            if (widget && widget->property(constOnCombo).toBool()) {
                painter->fillRect(r, option->palette.background());
            } else if (!widget || widget->testAttribute(Qt::WA_OpaquePaintEvent)) {
                painter->fillRect(r, option->palette.base());
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
    } else if (touchStyleSpin && CC_SpinBox==control) {
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            if (QAbstractSpinBox::NoButtons!=spinBox->buttonSymbols) {
                // Use PE_FrameLineEdit to draw border, as QGtkStyle corrupts focus settings
                // if its asked to draw a QSpinBox with no buttons that has focus.
                QStyleOptionFrameV2 opt;
                opt.state=spinBox->state;
                opt.state|=State_Sunken;
                opt.rect=spinBox->rect;
                opt.palette=spinBox->palette;
                opt.lineWidth=baseStyle()->pixelMetric(QStyle::PM_DefaultFrameWidth, option, 0);
                opt.midLineWidth=0;
                opt.fontMetrics=spinBox->fontMetrics;
                opt.direction=spinBox->direction;
                opt.type=QStyleOption::SO_Default;
                opt.version=1;
                baseStyle()->drawPrimitive(PE_FrameLineEdit, &opt, painter, 0);

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

void GtkProxyStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (PE_PanelScrollAreaCorner==element && SB_Thin==sbarType && option) {
        painter->fillRect(option->rect, option->palette.brush(QPalette::Base));
    } else {
        baseStyle()->drawPrimitive(element, option, painter, widget);
    }
}

void GtkProxyStyle::polish(QWidget *widget)
{
    if (SB_Standard!=sbarType && qobject_cast<QScrollBar *>(widget) && isOnCombo(widget)) {
        widget->setProperty(constOnCombo, true);
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
    baseStyle()->polish(widget);
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
