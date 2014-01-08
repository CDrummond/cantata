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
#ifdef ENABLE_OVERLAYSCROLLBARS
#include "osthumb.h"
#endif
#ifndef ENABLE_KDE_SUPPORT
#include "acceleratormanager.h"
#endif
#include <QComboBox>
#include <QSpinBox>
#include <QToolBar>
#include <QAbstractScrollArea>
#include <QAbstractItemView>
#include <QTreeView>
#include <QHeaderView>
#include <QScrollBar>
#include <QMenu>
#include <QApplication>
#include <QHoverEvent>
#include <QPainter>
#include <QDesktopWidget>
#include <QLibrary>

static const char * constOnCombo="on-combo";
#ifndef ENABLE_KDE_SUPPORT
static const char * constAccelProp="catata-accel";
#endif

//static bool revertQGtkStyleOverlayMod()
//{
//    // TODO: This is not working. ubuntu_gtk_set_use_overlay_scrollbar is found, and called.
//    // But, Gtk filechooser STILL has non-overlay scrollbars!!!
//    typedef void (*SetUseOsFn) (int);
//    // enforce the "0" suffix, so we'll open libgtk-x11-2.0.so.0
//    QLibrary libgtk(QLatin1String("gtk-x11-2.0"), 0, 0);
//    SetUseOsFn setUseOsFn = (SetUseOsFn)libgtk.resolve("ubuntu_gtk_set_use_overlay_scrollbar");

//    if (setUseOsFn) {
//        setUseOsFn(!0);
//        qputenv("LIBOVERLAY_SCROLLBAR", "override-blacklist");
//        return true;
//    }
//    return false;
//}


static bool isOnCombo(const QWidget *w)
{
    return w && (qobject_cast<const QComboBox *>(w) || isOnCombo(w->parentWidget()));
}

GtkProxyStyle::GtkProxyStyle(ScrollbarType sb, bool styleSpin)
    : QProxyStyle()
    #ifdef ENABLE_OVERLAYSCROLLBARS
    , sbarThumb(0)
    , sbarWidth(-1)
    , sbarAreaWidth(-1)
    , sbarOffset(0xffffffff)
    , sbarLastPos(-1)
    , sbarThumbTarget(0)
    #endif
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

        #ifdef ENABLE_OVERLAYSCROLLBARS
        if (SB_Overlay==sbarType && Qt::LeftToRight==QApplication::layoutDirection()) { //  && revertQGtkStyleOverlayMod()) {
            sbarWidth=qMax(fh/5, 3);
            sbarAreaWidth=sbarWidth*6;
            sbarThumb=new OsThumb();
            sbarThumb->setVisible(false);
            connect(sbarThumb, SIGNAL(thumbDragged(QPoint)), SLOT(sbarThumbMoved(QPoint)));
            connect(sbarThumb, SIGNAL(pageUp()), SLOT(sbarPageUp()));
            connect(sbarThumb, SIGNAL(pageDown()), SLOT(sbarPageDown()));
            connect(sbarThumb, SIGNAL(hiding()), SLOT(sbarThumbHiding()));
            connect(sbarThumb, SIGNAL(showing()),SLOT(sbarThumbShowing()));
        }
        #endif
    }
}

GtkProxyStyle::~GtkProxyStyle()
{
    #ifdef ENABLE_OVERLAYSCROLLBARS
    destroySliderThumb();
    #endif
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
        sz += QSize(0, 2);
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
        #ifdef ENABLE_OVERLAYSCROLLBARS
        return !sbarThumb || usePlainScrollbars(widget) ? sbarPlainViewWidth : sbarWidth;
        #else
        return sbarPlainViewWidth;
        #endif
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
    } else if (touchStyleSpin && CC_SpinBox==control) {
        if (const QStyleOptionSpinBox *spinBox = qstyleoption_cast<const QStyleOptionSpinBox *>(option)) {
            if (QAbstractSpinBox::NoButtons!=spinBox->buttonSymbols) {
                int border=2;
                int internalHeight=spinBox->rect.height()-(border*2);
                int internalWidth=internalHeight*1.25;
                switch (subControl) {
                case SC_SpinBoxUp:
                    return QRect(spinBox->rect.width()-(internalWidth+border), border, internalWidth, internalHeight);
                case SC_SpinBoxDown:
                    return QRect(spinBox->rect.width()-((internalWidth*2)+border), border, internalWidth, internalHeight);
                case SC_SpinBoxEditField:
                    return QRect(border, border, spinBox->rect.width()-((internalWidth*2)+border), internalHeight);
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

static void drawLine(QPainter *painter, QColor col, const QPoint &start, const QPoint &end)
{
    QLinearGradient grad(start, end);
    col.setAlphaF(0.0);
    grad.setColorAt(0, col);
    col.setAlphaF(0.25);
    grad.setColorAt(0.25, col);
    grad.setColorAt(0.8, col);
    col.setAlphaF(0.0);
    grad.setColorAt(1, col);
    painter->setPen(QPen(QBrush(grad), 1));
    painter->drawLine(start, end);
}

static void drawSpinButton(QPainter *painter, QRect rect, QColor &col, bool isPlus)
{
    int length=(rect.height()/4)-1;
    int lineWidth=(rect.height()-6)<32 ? 2 : 4;

    if (length<lineWidth) {
        length=lineWidth;
    }
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(QPen(col, lineWidth));

    rect.adjust(1, 1, 1, 1);
    painter->drawLine(rect.x()+((rect.width()/2)-(length+1)), rect.y()+((rect.height()-lineWidth)/2),
                      rect.x()+((rect.width()/2)+(length-1)), rect.y()+((rect.height()-lineWidth)/2));
    if (isPlus) {
        painter->drawLine(rect.x()+((rect.width()-lineWidth)/2), rect.y()+((rect.height()/2)-(length+1)),
                          rect.x()+((rect.width()-lineWidth)/2), rect.y()+((rect.height()/2)+(length-1)));
    }
    painter->restore();
}

const QAbstractItemView * view(const QWidget *w) {
    if (!w) {
        return 0;
    }
    const QAbstractItemView *v=qobject_cast<const QAbstractItemView *>(w);
    return v ? v : view(w->parentWidget());
}

void GtkProxyStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter, const QWidget *widget) const
{
    if (SB_Standard!=sbarType && CC_ScrollBar==control) {
        if (const QStyleOptionSlider *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            QRect r=option->rect;
            QRect slider=subControlRect(control, option, SC_ScrollBarSlider, widget);
            painter->save();
            #ifdef ENABLE_OVERLAYSCROLLBARS
            bool usePlain=!sbarThumb || usePlainScrollbars(widget);
            #else
            bool usePlain=true;
            #endif

            if (usePlain) {
                #if 0
                QLinearGradient grad(r.topLeft(), Qt::Horizontal==sb->orientation ? r.bottomLeft() : r.topRight());
                QColor col=option->palette.base().color();
                grad.setColorAt(0, col.darker(110));
                grad.setColorAt(1, col.darker(102));
                painter->fillRect(r, grad);
                #else
                if (widget && widget->property(constOnCombo).toBool()) {
                    painter->fillRect(r, option->palette.background());
                } else if (!widget || widget->testAttribute(Qt::WA_OpaquePaintEvent)) {
                    painter->fillRect(r, option->palette.base());
                }
                #endif
            }
            #ifdef ENABLE_OVERLAYSCROLLBARS
            else {
                painter->fillRect(r, usePlain ? option->palette.base() : option->palette.background());
                const QAbstractItemView *v=view(widget);
                if (v && qobject_cast<const QTreeView *>(v) && ((const QTreeView *)v)->header()&& ((const QTreeView *)v)->header()->isVisible()) {
                    QStyleOptionHeader ho;
                    ho.rect=QRect(r.x()+r.width()-(sbarWidth), r.y(), sbarWidth, ((const QTreeView *)v)->header()->height());
                    ho.state=option->state;
                    ho.palette=option->palette;
                    painter->save();
                    drawControl(CE_Header, &ho, painter, ((const QTreeView *)v)->header());
                    painter->restore();
                }
            }
            #endif
            if (slider.isValid()) {
                if (usePlain) {
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
                #ifdef ENABLE_OVERLAYSCROLLBARS
                else {
                    QRect toThumb;
                    QPalette::ColorGroup cg=option->palette.currentColorGroup();
                    if (sbarThumb && sbarThumbTarget && sbarThumbTarget==widget && sbarThumb->isVisible()) {
                        QPoint p=sbarThumbTarget->mapFromGlobal(sbarThumb->pos())+QPoint(1, 1);
                        if (Qt::Horizontal==sb->orientation) {
                            if (p.x()<slider.x()) {
                                toThumb=QRect(p.x(), slider.y(), slider.x()-p.x(), slider.width());
                            } else if (p.x()>(slider.x()+slider.width())) {
                                toThumb=QRect(slider.x()+slider.width(), slider.x(), p.x()-(slider.y()+slider.width()), slider.width());
                            }
                        } else {
                            if (p.y()<slider.y()) {
                                toThumb=QRect(slider.x(), p.y(), slider.width(), slider.y()-p.y());
                            } else if (p.y()>(slider.y()+slider.height())) {
                                toThumb=QRect(slider.x(), slider.y()+slider.height(), slider.width(), p.y()-(slider.y()+slider.height()));
                            }
                        }
                        cg=QPalette::Active;
                    }
                    if (toThumb.isValid()) {
                        QColor col(option->palette.color(cg, QPalette::Text));
                        col.setAlphaF(0.35);
                        painter->fillRect(toThumb, col);
                    }
                    painter->fillRect(slider, option->palette.color(cg, QPalette::Highlight));
                }
                #endif
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
                QColor col(spinBox->palette.foreground().color());
                drawLine(painter, col, plusRect.topLeft(), plusRect.bottomLeft());
                drawLine(painter, col, minusRect.topLeft(), minusRect.bottomLeft());

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
                        painter->fillRect(fillRect.adjusted(1, 2, -1, -2), col);
                    }
                }
                drawSpinButton(painter, plusRect, col, true);
                drawSpinButton(painter, minusRect, col, false);
                return;
            }
        }
    }
    baseStyle()->drawComplexControl(control, option, painter, widget);
}

#ifdef ENABLE_OVERLAYSCROLLBARS
static inline void addEventFilter(QObject *object, QObject *filter)
{
    object->removeEventFilter(filter);
    object->installEventFilter(filter);
}

void GtkProxyStyle::destroySliderThumb()
{
    if (sbarThumb) {
        sbarThumb->setVisible(false);
        sbarThumb->deleteLater();
        sbarThumb=0;
    }
}
#endif

inline void addEventFilter(QObject *object, QObject *filter)
{
    object->removeEventFilter(filter);
    object->installEventFilter(filter);
}

void GtkProxyStyle::polish(QWidget *widget)
{
    #ifdef ENABLE_OVERLAYSCROLLBARS
    if (SB_Overlay==sbarType && sbarThumb && widget && qobject_cast<QAbstractScrollArea *>(widget) && qstrcmp(widget->metaObject()->className(), "QComboBoxListView")) {
        addEventFilter(widget, this);
        widget->setAttribute(Qt::WA_Hover, true);
    } else if (SB_Standard!=sbarType && usePlainScrollbars(widget)) {
        widget->setAttribute(Qt::WA_Hover, true);
    }
    #else

//    TODO: Why was this here?? Should it not have just been for QScrollBars???
//    if (SB_Standard!=sbarType) {
//        widget->setAttribute(Qt::WA_Hover, true);
//    }
    if (SB_Standard!=sbarType && qobject_cast<QScrollBar *>(widget) && isOnCombo(widget)) {
        widget->setProperty(constOnCombo, true);
    }
    #endif

    #ifndef ENABLE_KDE_SUPPORT
    if (widget && qobject_cast<QMenu *>(widget) && !widget->property(constAccelProp).isValid()) {
        AcceleratorManager::manage(widget);
        widget->setProperty(constAccelProp, true);
    }
    #endif
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
    #ifdef ENABLE_OVERLAYSCROLLBARS
    if (SB_Overlay==sbarType && sbarThumb && widget) {
        if (qobject_cast<QAbstractScrollArea *>(widget) && qstrcmp(widget->metaObject()->className(), "QComboBoxListView")) {
            widget->removeEventFilter(this);
        }
        if (sbarThumb && sbarThumbTarget==widget) {
            sbarThumb->hide();
        }
    }
    #endif
    baseStyle()->unpolish(widget);
}

void GtkProxyStyle::unpolish(QApplication *app)
{
    app->removeEventFilter(shortcutHander);
    baseStyle()->unpolish(app);
}

#ifdef ENABLE_OVERLAYSCROLLBARS
bool GtkProxyStyle::eventFilter(QObject *object, QEvent *event)
{
    if (SB_Overlay==sbarType) {
        switch (event->type()) {
        case QEvent::HoverMove:
            if (object && qobject_cast<QAbstractScrollArea *>(object)) {
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
                    if (sbarThumbTarget!=bar) {
                        if (sbarThumbTarget) {
                            disconnect(bar, SIGNAL(destroyed(QObject *)), this, SLOT(objectDestroyed(QObject *)));
                        }
                        sbarThumbTarget=bar;
                        connect(bar, SIGNAL(destroyed(QObject *)), this, SLOT(objectDestroyed(QObject *)));
                    }

                    if (!sbarThumb->isVisible() || bar->orientation()!=sbarThumb->orientation()) {
                        sbarThumb->setOrientation(bar->orientation());
                        QPoint global=a->mapToGlobal(QPoint(Qt::Vertical==bar->orientation() ? a->width()-sbarWidth-1 : 0, Qt::Vertical==bar->orientation() ? 0 : a->height()-sbarWidth-1));
                        int toXPos=global.x();
                        int toYPos=global.y();

                        if (Qt::Vertical==bar->orientation()) {
                            int thumbSize = sbarThumb->height();
                            toYPos = a->mapToGlobal(he->pos()).y() - sbarThumb->height() / 2;
                            int minYPos = global.y();
                            int maxYPos = global.y() + a->height() - thumbSize;

                            sbarThumb->setMaximum(maxYPos);
                            sbarThumb->setMinimum(minYPos);

                            if (toYPos < minYPos) {
                                toYPos = minYPos;
                            }
                            if (toYPos > maxYPos) {
                                toYPos = maxYPos;
                            }
                            if (QApplication::desktop() && toXPos+sbarThumb->width()>QApplication::desktop()->width()) {
                                toXPos=global.x()-(sbarThumb->width()-sbarWidth);
                            }
                        } else {
                            int thumbSize = sbarThumb->height();
                            toXPos = a->mapToGlobal(he->pos()).x() - sbarThumb->width() / 2;
                            int minXPos = global.x();
                            int maxXPos = global.x() + a->width() - thumbSize;

                            sbarThumb->setMaximum(maxXPos);
                            sbarThumb->setMinimum(minXPos);

                            if (toXPos < minXPos) {
                                toXPos = minXPos;
                            }
                            if (toXPos > maxXPos) {
                                toXPos = maxXPos;
                            }

                            if (QApplication::desktop() && toYPos+sbarThumb->height()>QApplication::desktop()->height()) {
                                toYPos=global.y()-(sbarThumb->height()-sbarWidth);
                            }
                        }
                        sbarThumb->move(toXPos, toYPos);
                        sbarThumb->show();
                        sbarUpdateOffset();
                    }
                } else {
                    sbarThumb->hide();
                }
            }
            break;
        case QEvent::Resize:
        case QEvent::Move:
            if (sbarThumb && sbarThumb->isVisible() && object && qobject_cast<QAbstractScrollArea *>(object)) {
                sbarThumb->hide();
            }
            break;
        case QEvent::WindowDeactivate:
            if (sbarThumb && sbarThumb->isVisible()) {
                sbarThumb->hide();
            }
        default:
            break;
        }
    }

    return baseStyle()->eventFilter(object, event);
}

void GtkProxyStyle::objectDestroyed(QObject *obj)
{
    if (obj==sbarThumbTarget) {
        disconnect(sbarThumbTarget, SIGNAL(destroyed(QObject *)), this, SLOT(objectDestroyed(QObject *)));
        sbarThumbTarget=0;
    }
}

void GtkProxyStyle::sbarThumbMoved(const QPoint &point)
{
    if (sbarThumbTarget) {
        bool v=Qt::Vertical==sbarThumbTarget->orientation();
        if (-1!=sbarLastPos && sbarLastPos==(v ? point.y() : point.x())) {
            return;
        }
        QPoint global=point-sbarThumbTarget->mapToGlobal(QPoint(0, 0))-QPoint(1, 1);
        int value=sbarThumbTarget->value();
        QRect sliderThumbRect=sbarGetSliderRect();
        QRect osThumbRect=QRect(sbarThumb->x(), sbarThumb->y(), sbarThumb->width(), sbarThumb->height());
        int adjust=osThumbRect.contains(sliderThumbRect) ? 0 : sbarOffset;

        if (v) {
            sbarThumbTarget->setValue(sliderValueFromPosition(sbarThumbTarget->minimum(), sbarThumbTarget->maximum(), global.y() -adjust,
                                                              sbarThumbTarget->height() - sbarThumb->height()));
            sbarLastPos=point.y();
        } else {
            sbarThumbTarget->setValue(sliderValueFromPosition(sbarThumbTarget->minimum(), sbarThumbTarget->maximum(), global.x() -adjust,
                                                              sbarThumbTarget->width() - sbarThumb->width()));
            sbarLastPos=point.x();
        }
        if (value==sbarThumbTarget->value()) {
            sbarThumbTarget->update();
        }
    }
}

void GtkProxyStyle::sbarPageUp()
{
    if (sbarThumbTarget) {
        sbarThumbTarget->setValue(sbarThumbTarget->value() - sbarThumbTarget->pageStep());

        if (sbarThumbTarget->value() < sbarThumbTarget->minimum()) {
            sbarThumbTarget->setValue(sbarThumbTarget->minimum());
        }
        sbarUpdateOffset();
    }
}

void GtkProxyStyle::sbarPageDown()
{
    if (sbarThumbTarget) {
        sbarThumbTarget->setValue(sbarThumbTarget->value() + sbarThumbTarget->pageStep());

        if (sbarThumbTarget->value() > sbarThumbTarget->maximum()) {
            sbarThumbTarget->setValue(sbarThumbTarget->maximum());
        }
        sbarUpdateOffset();
    }
}

void GtkProxyStyle::sbarThumbHiding()
{
    if (sbarThumbTarget) {
        sbarThumbTarget->update();
    }
    sbarLastPos=-1;
}

void GtkProxyStyle::sbarThumbShowing()
{
    if (sbarThumbTarget) {
        sbarThumbTarget->update();
    }
}

QRect GtkProxyStyle::sbarGetSliderRect() const
{
    QStyleOptionSlider opt;
    opt.initFrom(sbarThumbTarget);
    opt.subControls = QStyle::SC_None;
    opt.activeSubControls = QStyle::SC_None;
    opt.orientation = sbarThumbTarget->orientation();
    opt.minimum = sbarThumbTarget->minimum();
    opt.maximum = sbarThumbTarget->maximum();
    opt.sliderPosition = sbarThumbTarget->sliderPosition();
    opt.sliderValue = sbarThumbTarget->value();
    opt.singleStep = sbarThumbTarget->singleStep();
    opt.pageStep = sbarThumbTarget->pageStep();
    opt.upsideDown = sbarThumbTarget->invertedAppearance();
    if (Qt::Horizontal==sbarThumbTarget->orientation()) {
        opt.state |= QStyle::State_Horizontal;
    }

    QRect slider=subControlRect(CC_ScrollBar, &opt, SC_ScrollBarSlider, sbarThumbTarget);
    QPoint gpos=sbarThumbTarget->mapToGlobal(slider.topLeft())-QPoint(1, 1);
    return QRect(gpos.x(), gpos.y(), slider.width(), slider.height());
}

void GtkProxyStyle::sbarUpdateOffset()
{
    QRect sr=sbarGetSliderRect();

    if (Qt::Vertical==sbarThumbTarget->orientation()) {
        sbarOffset=sbarThumb->pos().y()-sr.y();
    } else {
        sbarOffset=sbarThumb->pos().x()-sr.x();
    }
}

bool GtkProxyStyle::usePlainScrollbars(const QWidget *widget) const
{
    return SB_Thin==sbarType || !widget || 0==qstrcmp(widget->metaObject()->className(), "QWebView") ||
           (widget && widget->parentWidget() && widget->parentWidget()->parentWidget() &&
            0==qstrcmp(widget->parentWidget()->parentWidget()->metaObject()->className(), "QComboBoxListView"));
}
#endif
