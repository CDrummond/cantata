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

#ifndef TOUCH_SCROLL_H
#define TOUCH_SCROLL_H

#include "config.h"

#ifdef CANTATA_ANDROID

#include <QtGui/QScrollBar>
#include <QtGui/QCursor>
#include <QtGui/QMenu>
#include <QtCore/QTimer>
#include <QtCore/QDateTime>
#include <math.h>

#define TOUCH_SETUP \
    viewport()->setAttribute(Qt::WA_AcceptTouchEvents); setVerticalScrollMode(QAbstractItemView::ScrollPerPixel); \
    touchActive=false; \
    touchType=TouchNone; \
    touchSize=fontMetrics().height(); \
    touchLastUsed=0.0; \
    touchTimer=0;

#define TOUCH_MEMBERS \
    bool viewportEvent(QEvent *event); \
    enum TouchType { TouchNone, TouchScroll, TouchMenu }; \
    int touchSize; \
    bool touchActive; \
    TouchType touchType; \
    double touchLastUsed; \
    QTimer *touchTimer; \
    QTime touchTime;

#define TOUCH_CONTEXT_MENU(CLASS) \
    void CLASS::showTouchContextMenu() \
    { \
        if (TouchNone==touchType) { \
            touchType=TouchMenu; \
            if (Qt::ActionsContextMenu==contextMenuPolicy() && !actions().isEmpty()) { \
                QMenu::exec(actions(), QCursor::pos()); \
            } \
        } \
    }

#define TOUCH_EVENT_HANDLER(CLASS) \
    bool CLASS::viewportEvent(QEvent *event) { \
        switch (event->type()) { \
        case QEvent::TouchBegin: { \
            QTouchEvent *te=(QTouchEvent *)event; \
            if (1==te->touchPoints().count()) { \
                touchActive=true; \
                touchType=TouchNone; \
                touchLastUsed=te->touchPoints().at(0).pos().y(); \
                if (!touchTimer) { \
                    touchTimer=new QTimer(this); \
                    connect(touchTimer, SIGNAL(timeout()), SLOT(showTouchContextMenu())); \
                } \
                touchTimer->start(1500); \
                touchTime.restart(); \
            } \
            return true; \
        } \
        case QEvent::TouchEnd: \
            if (touchActive) { \
                touchActive=false; \
                if (touchTimer) { \
                    touchTimer->stop(); \
                } \
                if (TouchMenu!=touchType) { \
                    QTouchEvent *te=(QTouchEvent *)event; \
                    if (1==te->touchPoints().count()) { \
                        QPointF pos=te->touchPoints().at(0).pos(); \
                        QPointF touchStart=te->touchPoints().at(0).startPos(); \
                        if (pos.x()<touchStart.x() && fabs(pos.x()-touchStart.x())>(touchSize*6) && fabs(pos.y()-touchStart.y())<(touchSize*3)) { \
                            touchType=TouchScroll; \
                            emit goBack(); \
                        } \
                    } \
                } \
            } \
            return true; \
        case QEvent::TouchUpdate: \
            if (touchActive && TouchMenu!=touchType) { \
                QTouchEvent *te=(QTouchEvent *)event; \
                if (1==te->touchPoints().count()) { \
                    QScrollBar *sb=verticalScrollBar(); \
                    if (sb) { \
                        QTouchEvent::TouchPoint point=te->touchPoints().at(0); \
                        if (point.pos().y()>-1 && point.pos().y()<height()) { \
                            double diff=touchLastUsed-point.pos().y(); \
                            if (diff>1 || diff<-1) { \
                                int elapsed=touchTime.elapsed(); \
                                if (elapsed<2) { \
                                    diff*=4.5; \
                                } else if (elapsed<5) { \
                                    diff*=3.25; \
                                } else if (elapsed<10) { \
                                    diff*=2.25; \
                                } else if (elapsed<15) { \
                                    diff*=1.5; \
                                } \
                                touchLastUsed=point.pos().y(); \
                                int val=sb->value()+diff; \
                                if (touchTimer) { \
                                    touchTimer->stop(); \
                                } \
                                touchTime.restart(); \
                                QPointF touchStart=te->touchPoints().at(0).startPos(); \
                                if (fabs(point.pos().y()-touchStart.y())>touchSize || fabs(point.pos().x()-touchStart.x())>touchSize) { \
                                    touchType=TouchScroll; \
                                } \
                                sb->setValue(val<=sb->minimum() ? sb->minimum() : (val>=sb->maximum() ? sb->maximum() : val)); \
                            } \
                        } \
                    } \
                } \
            } \
            return true; \
        default: \
            break; \
        } \
        return Q##CLASS::viewportEvent(event); \
    }

#else

#define TOUCH_SETUP
#define TOUCH_MEMBERS
#define TOUCH_CONTEXT_MENU(CLASS) void CLASS::showTouchContextMenu() { }
#define TOUCH_EVENT_HANDLER(A)

#endif

#endif

