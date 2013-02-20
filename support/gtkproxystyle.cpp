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
#include "osthumb.h"
#include "config.h"
#include <QComboBox>
#include <QToolBar>
#include <QAbstractScrollArea>
#include <QScrollBar>
#include <QApplication>
#include <QHoverEvent>
#include <QPainter>
#include <QDesktopWidget>
#include <QLibrary>

const char * GtkProxyStyle::constSlimComboProperty="gtkslim";

static bool revertQGtkStyleOverlayMod()
{
    // TODO: This is not working. ubuntu_gtk_set_use_overlay_scrollbar is found, and called.
    // But, Gtk filechooser STILL has non-overlay scrollbars!!!
    typedef void (*SetUseOsFn) (int);
    // enforce the "0" suffix, so we'll open libgtk-x11-2.0.so.0
    QLibrary libgtk(QLatin1String("gtk-x11-2.0"), 0, 0);
    SetUseOsFn setUseOsFn = (SetUseOsFn)libgtk.resolve("ubuntu_gtk_set_use_overlay_scrollbar");;

    if (setUseOsFn) {
        setUseOsFn(!0);
        return true;
    }
    return false;
}

GtkProxyStyle::GtkProxyStyle(bool overlaySBars)
    : QProxyStyle()
    , thumb(0)
    , sbarWidth(-1)
    , sbarAreaWidth(-1)
    , sliderOffset(0xffffffff)
    , thumbTarget(0)
{
    useOverlayScrollbars=overlaySBars && qgetenv("LIBOVERLAY_SCROLLBAR")!="0";
    setBaseStyle(qApp->style());
    toolbarCombo=new QComboBox(new QToolBar());
    if (useOverlayScrollbars) {
        int fh=QApplication::fontMetrics().height();
        sbarWebViewWidth=fh/1.75;

        if (Qt::LeftToRight==QApplication::layoutDirection() && revertQGtkStyleOverlayMod()) {
            sbarWidth=qMax(fh/5, 3);
            sbarAreaWidth=sbarWidth*4;
            thumb=new OsThumb();
            thumb->setVisible(false);
            connect(thumb, SIGNAL(thumbDragged(QPoint)), SLOT(thumbMoved(QPoint)));
            connect(thumb, SIGNAL(pageUp()), SLOT(sbarPageUp()));
            connect(thumb, SIGNAL(pageDown()), SLOT(sbarPageDown()));
        }
    }
}

GtkProxyStyle::~GtkProxyStyle()
{
    destroySliderThumb();
}

static bool isWebView(const QWidget *widget)
{
    return !widget || 0==qstrcmp(widget->metaObject()->className(), "QWebView");
}

QSize GtkProxyStyle::sizeFromContents(ContentsType type, const QStyleOption *option,  const QSize &size, const QWidget *widget) const
{
    QSize sz=baseStyle()->sizeFromContents(type, option, size, widget);

    if (CT_ComboBox==type && widget && widget->property(constSlimComboProperty).toBool()) {
        QSize orig=baseStyle()->sizeFromContents(type, option, size, widget);
        QSize other=baseStyle()->sizeFromContents(type, option, size, toolbarCombo);
        if (orig.height()>other.height()) {
            return QSize(orig.width(), other.height());
        }
        return orig;
    } else if (useOverlayScrollbars && CT_ScrollBar==type) {
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
    if (useOverlayScrollbars && SH_ScrollView_FrameOnlyAroundContents==hint) {
        return false;
    }

    return baseStyle()->styleHint(hint, option, widget, returnData);
}

int GtkProxyStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    if (useOverlayScrollbars && PM_ScrollBarExtent==metric) {
        return !thumb || isWebView(widget) ? sbarWebViewWidth : sbarWidth;
    }
    return baseStyle()->pixelMetric(metric, option, widget);
}

QRect GtkProxyStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl, const QWidget *widget) const
{
    if (useOverlayScrollbars && CC_ScrollBar==control) {
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
    if (useOverlayScrollbars && CC_ScrollBar==control) {
        if (const QStyleOptionSlider *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            QRect r=option->rect;
            QRect slider=subControlRect(control, option, SC_ScrollBarSlider, widget);
            painter->save();
            painter->fillRect(r, option->palette.base());
            bool webView=!thumb || isWebView(widget);

            if (webView) {
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
            }
            if (slider.isValid()) {
                if (webView) {
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
                } else {
                    QRect toThumb;
                    if (thumb && thumbTarget && thumbTarget==widget && thumb->isVisible()) {
                        QPoint p=thumbTarget->mapFromGlobal(thumb->pos())+QPoint(1, 1);
                        if (Qt::Horizontal==sb->orientation) {
                            if (p.x()<slider.x()) {
                                toThumb=QRect(p.x(), slider.y(), slider.x()-p.x(), slider.width());
                            } else if (p.x()>(slider.x()+slider.width())) {
                                toThumb=QRect(slider.x()+slider.width(), slider.x(), p.x()-slider.y(), slider.width());
                            }
                        } else {
                            if (p.y()<slider.y()) {
                                toThumb=QRect(slider.x(), p.y(), slider.width(), slider.y()-p.y());
                            } else if (p.y()>(slider.y()+slider.height())) {
                                toThumb=QRect(slider.x(), slider.y()+slider.height(), slider.width(), p.y()-slider.y());
                            }
                        }
                    }
                    if (toThumb.isValid()) {
                        QColor col(option->palette.text().color());
                        col.setAlphaF(0.35);
                        painter->fillRect(toThumb, col);
                    }
                    painter->fillRect(slider, option->palette.highlight().color());
                }
            }

            painter->restore();
            return;
        }
    }
    baseStyle()->drawComplexControl(control, option, painter, widget);
}

static inline void addEventFilter(QObject *object, QObject *filter)
{
    object->removeEventFilter(filter);
    object->installEventFilter(filter);
}

void GtkProxyStyle::polish(QWidget *widget)
{
    if (useOverlayScrollbars && thumb && widget && qobject_cast<QAbstractScrollArea *>(widget)) {
        addEventFilter(widget, this);
        widget->setAttribute(Qt::WA_Hover, true);
    } else if (useOverlayScrollbars && isWebView(widget)) {
        widget->setAttribute(Qt::WA_Hover, true);
    }
    baseStyle()->polish(widget);
}

void GtkProxyStyle::unpolish(QWidget *widget)
{
    if (useOverlayScrollbars && thumb && widget) {
        if (qobject_cast<QAbstractScrollArea *>(widget)) {
            widget->removeEventFilter(this);
        }
        if (thumb && thumbTarget==widget) {
            thumb->hide();
        }
    }
    baseStyle()->unpolish(widget);
}

void GtkProxyStyle::destroySliderThumb()
{
    if (thumb) {
        thumb->setVisible(false);
        thumb->deleteLater();
        thumb=0;
    }
}

bool GtkProxyStyle::eventFilter(QObject *object, QEvent *event)
{
    if (useOverlayScrollbars && QEvent::HoverMove==event->type() && object && qobject_cast<QAbstractScrollArea *>(object)) {
        QHoverEvent *he=(QHoverEvent *)event;
        QAbstractScrollArea *a=(QAbstractScrollArea *)object;
        QScrollBar *bar=0;
        if (a->horizontalScrollBar() && a->horizontalScrollBar()->isVisible() && he->pos().y()>(a->height()-sbarAreaWidth)) {
            bar=a->horizontalScrollBar();
        }

        if (a->verticalScrollBar() && a->verticalScrollBar()->isVisible() && he->pos().x()>(a->width()-sbarAreaWidth)) {
            bar=a->verticalScrollBar();
        }

        if (bar) {
            if (thumbTarget!=bar) {
                if (thumbTarget) {
                    disconnect(bar, SIGNAL(destroyed(QObject *)), this, SLOT(objectDestroyed(QObject *)));
                    disconnect(thumb, SIGNAL(hidding()), thumbTarget, SLOT(update()));
                    disconnect(thumb, SIGNAL(showing()), thumbTarget, SLOT(update()));
                }
                thumbTarget=bar;
                connect(bar, SIGNAL(destroyed(QObject *)), this, SLOT(objectDestroyed(QObject *)));
                connect(thumb, SIGNAL(hidding()), thumbTarget, SLOT(update()));
                connect(thumb, SIGNAL(showing()), thumbTarget, SLOT(update()));
            }

            thumb->setOrientation(bar->orientation());

            if (!thumb->isVisible()) {
                QPoint global=a->mapToGlobal(QPoint(Qt::Vertical==bar->orientation() ? a->width()-sbarWidth-1 : 0, Qt::Vertical==bar->orientation() ? 0 : a->height()-sbarWidth-1));
                int toXPos=global.x();
                int toYPos=global.y();

                if (Qt::Vertical==bar->orientation()) {
                    int thumbSize = thumb->height();
                    toYPos = a->mapToGlobal(he->pos()).y() - thumb->height() / 2;
                    int minYPos = global.y();
                    int maxYPos = global.y() + a->height() - thumbSize;

                    thumb->setMaximum(maxYPos);
                    thumb->setMinimum(minYPos);

                    if (toYPos < minYPos) {
                        toYPos = minYPos;
                    }
                    if (toYPos > maxYPos) {
                        toYPos = maxYPos;
                    }
                    if (QApplication::desktop() && toXPos+thumb->width()>QApplication::desktop()->width()) {
                        toXPos=global.x()-(thumb->width()-sbarWidth);
                    }
                    int sliderPos=sliderPositionFromValue(thumbTarget->minimum(), thumbTarget->maximum(), thumbTarget->value(), thumbTarget->height() - thumb->height());
                    sliderOffset=toYPos-(global.y()+sliderPos);
                } else {
                    int thumbSize = thumb->height();
                    toXPos = a->mapToGlobal(he->pos()).x() - thumb->width() / 2;
                    int minXPos = global.x();
                    int maxXPos = global.x() + a->width() - thumbSize;

                    thumb->setMaximum(maxXPos);
                    thumb->setMinimum(minXPos);

                    if (toXPos < minXPos) {
                        toXPos = minXPos;
                    }
                    if (toXPos > maxXPos) {
                        toXPos = maxXPos;
                    }

                    if (QApplication::desktop() && toYPos+thumb->height()>QApplication::desktop()->height()) {
                        toYPos=global.y()-(thumb->height()-sbarWidth);
                    }
                    int sliderPos=sliderPositionFromValue(thumbTarget->minimum(), thumbTarget->maximum(), thumbTarget->value(), thumbTarget->width() - thumb->width());
                    sliderOffset=toXPos-(global.x()+sliderPos);
                }
                thumb->move(toXPos, toYPos);
                thumb->show();
            }
        } else {
            thumb->hide();
        }
    } else if (useOverlayScrollbars && thumb && QEvent::WindowDeactivate==event->type() && thumb->isVisible()) {
        thumb->hide();
    }

    return baseStyle()->eventFilter(object, event);
}

void GtkProxyStyle::objectDestroyed(QObject *obj)
{
    if (obj==thumbTarget) {
        disconnect(thumbTarget, SIGNAL(destroyed(QObject *)), this, SLOT(objectDestroyed(QObject *)));
        thumbTarget=0;
    }
}

void GtkProxyStyle::thumbMoved(const QPoint &point)
{
    if (thumbTarget) {
        QPoint global=thumbTarget->mapToGlobal(QPoint(0, 0));
        int value=thumbTarget->value();
        if (Qt::Vertical==thumbTarget->orientation()) {
            thumbTarget->setValue(sliderValueFromPosition(thumbTarget->minimum(), thumbTarget->maximum(), point.y() - (global.y()+sliderOffset), thumbTarget->height() - thumb->height()));
            if ((sliderOffset>0 && point.y()<=global.y()) || (sliderOffset<0 && ((point.y()+thumb->height())>=(global.y()+thumbTarget->height())))) {
                sliderOffset=0;
            }
        } else {
            thumbTarget->setValue(sliderValueFromPosition(thumbTarget->minimum(), thumbTarget->maximum(), point.x() - global.x(), thumbTarget->width() - thumb->width()));
            if ((sliderOffset>0 && point.x()<=global.x()) || (sliderOffset<0 && ((point.x()+thumb->width())>=(global.x()+thumbTarget->width())))) {
                sliderOffset=0;
            }
        }
        if (value==thumbTarget->value()) {
            thumbTarget->update();
        }
    }
}

void GtkProxyStyle::sbarPageUp()
{
    if (thumbTarget) {
        thumbTarget->setValue(thumbTarget->value() - thumbTarget->pageStep());

        if (thumbTarget->value() < thumbTarget->minimum()) {
            thumbTarget->setValue(thumbTarget->minimum());
        }
        updateSliderOffset();
    }
}

void GtkProxyStyle::sbarPageDown()
{
    if (thumbTarget) {
        thumbTarget->setValue(thumbTarget->value() + thumbTarget->pageStep());

        if (thumbTarget->value() > thumbTarget->maximum()) {
            thumbTarget->setValue(thumbTarget->maximum());
        }
        updateSliderOffset();
    }
}

void GtkProxyStyle::updateSliderOffset()
{
    QPoint global=thumbTarget->mapToGlobal(QPoint(0, 0));
    if (Qt::Vertical==thumbTarget->orientation()) {
        int sliderPos=sliderPositionFromValue(thumbTarget->minimum(), thumbTarget->maximum(), thumbTarget->value(), thumbTarget->height() - thumb->height());
        sliderOffset=thumb->pos().y()-(global.y()+sliderPos);
    } else {
        int sliderPos=sliderPositionFromValue(thumbTarget->minimum(), thumbTarget->maximum(), thumbTarget->value(), thumbTarget->width() - thumb->width());
        sliderOffset=thumb->pos().x()-(global.x()+sliderPos);
    }
}
