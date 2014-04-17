/****************************************************************************
 **
 ** Copyright (C) 2013 Digia Plc and/or its subsidiary(-ies).
 ** Contact: http://www.qt-project.org/legal
 **
 ** This file is part of the demos of the Qt Toolkit.
 **
 ** $QT_BEGIN_LICENSE:LGPL$
 ** Commercial License Usage
 ** Licensees holding valid commercial Qt licenses may use this file in
 ** accordance with the commercial license agreement provided with the
 ** Software or, alternatively, in accordance with the terms contained in
 ** a written agreement between you and Digia.  For licensing terms and
 ** conditions see http://qt.digia.com/licensing.  For further information
 ** use the contact form at http://qt.digia.com/contact-us.
 **
 ** GNU Lesser General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU Lesser
 ** General Public License version 2.1 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.LGPL included in the
 ** packaging of this file.  Please review the following information to
 ** ensure the GNU Lesser General Public License version 2.1 requirements
 ** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
 **
 ** In addition, as a special exception, Digia gives you certain additional
 ** rights.  These rights are described in the Digia Qt LGPL Exception
 ** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
 **
 ** GNU General Public License Usage
 ** Alternatively, this file may be used under the terms of the GNU
 ** General Public License version 3.0 as published by the Free Software
 ** Foundation and appearing in the file LICENSE.GPL included in the
 ** packaging of this file.  Please review the following information to
 ** ensure the GNU General Public License version 3.0 requirements will be
 ** met: http://www.gnu.org/copyleft/gpl.html.
 **
 **
 ** $QT_END_LICENSE$
 **
 ****************************************************************************/

#include "flickcharm.h"
#include "globalstatic.h"
#include "settings.h"
#include <QAbstractScrollArea>
#include <QApplication>
#include <QBasicTimer>
#include <QEvent>
#include <QHash>
#include <QList>
#include <QMouseEvent>
#include <QScrollBar>
#include <QTime>
//#include <QWebFrame>
//#include <QWebView>

//#include <QDebug>

GLOBAL_STATIC(FlickCharm, instance)

const int fingerAccuracyThreshold = 3;

struct FlickData {
    typedef enum {
        Steady, // Interaction without scrolling
        ManualScroll, // Scrolling manually with the finger on the screen
        AutoScroll, // Scrolling automatically
        AutoScrollAcceleration // Scrolling automatically but a finger is on the screen
    } State;
    State state;
    QWidget *widget;
    QPoint pressPos;
    QPoint lastPos;
    QPoint speed;
    QTime speedTimer;
    QList<QEvent*> ignored;
    QTime accelerationTimer;
    bool lastPosValid:1;
    bool waitingAcceleration:1;

    FlickData()
        : lastPosValid(false)
        , waitingAcceleration(false)
    {}

    void resetSpeed()
    {
        speed = QPoint();
        lastPosValid = false;
    }
    void updateSpeed(const QPoint &newPosition)
    {
        if (lastPosValid) {
            const int timeElapsed = speedTimer.elapsed();
            if (timeElapsed) {
                const QPoint newPixelDiff = (newPosition - lastPos);
                const QPoint pixelsPerSecond = newPixelDiff * (1000 / timeElapsed);
                // fingers are inacurates, we ignore small changes to avoid stopping the autoscroll because
                // of a small horizontal offset when scrolling vertically
                const int newSpeedY = (qAbs(pixelsPerSecond.y()) > fingerAccuracyThreshold) ? pixelsPerSecond.y() : 0;
                const int newSpeedX = (qAbs(pixelsPerSecond.x()) > fingerAccuracyThreshold) ? pixelsPerSecond.x() : 0;
                if (state == AutoScrollAcceleration) {
                    const int max = 4000; // px by seconds
                    const int oldSpeedY = speed.y();
                    const int oldSpeedX = speed.x();
                    if (((oldSpeedY <= 0 && newSpeedY <= 0) ||  (oldSpeedY >= 0 && newSpeedY >= 0))
                            && ((oldSpeedX <= 0 && newSpeedX <= 0) ||  (oldSpeedX >= 0 && newSpeedX >= 0))) {
                        speed.setY(qBound(-max, (oldSpeedY + (newSpeedY / 4)), max));
                        speed.setX(qBound(-max, (oldSpeedX + (newSpeedX / 4)), max));
                    } else {
                        speed = QPoint();
                    }
                } else {
                    const int max = 2500; // px by seconds
                    // we average the speed to avoid strange effects with the last delta
                    if (!speed.isNull()) {
                        speed.setX(qBound(-max, (speed.x() / 4) + (newSpeedX * 3 / 4), max));
                        speed.setY(qBound(-max, (speed.y() / 4) + (newSpeedY * 3 / 4), max));
                    } else {
                        speed = QPoint(newSpeedX, newSpeedY);
                    }
                }
            }
        } else {
            lastPosValid = true;
        }
        speedTimer.start();
        lastPos = newPosition;
    }

    // scroll by dx, dy
    // return true if the widget was scrolled
    bool scrollWidget(const int dx, const int dy)
    {
        QAbstractScrollArea *scrollArea = qobject_cast<QAbstractScrollArea*>(widget);
        if (scrollArea) {
            const int x = scrollArea->horizontalScrollBar()->value();
            const int y = scrollArea->verticalScrollBar()->value();
            scrollArea->horizontalScrollBar()->setValue(x - dx);
            scrollArea->verticalScrollBar()->setValue(y - dy);
            return (scrollArea->horizontalScrollBar()->value() != x
                    || scrollArea->verticalScrollBar()->value() != y);
        }

        /*
         QWebView *webView = qobject_cast<QWebView*>(widget);
         if (webView) {
             QWebFrame *frame = webView->page()->mainFrame();
             const QPoint position = frame->scrollPosition();
             frame->setScrollPosition(position - QPoint(dx, dy));
             return frame->scrollPosition() != position;
         }
         */
        return false;
    }

    bool scrollTo(const QPoint &newPosition)
    {
        const QPoint delta = newPosition - lastPos;
        updateSpeed(newPosition);
        return scrollWidget(delta.x(), delta.y());
    }
};

class FlickCharmPrivate
{
public:
    QHash<QWidget*, FlickData*> flickData;
    QBasicTimer ticker;
    QTime timeCounter;
    void startTicker(QObject *object)
    {
        if (!ticker.isActive())
            ticker.start(15, object);
        timeCounter.start();
    }
};

FlickCharm::FlickCharm(QObject *parent): QObject(parent)
{
    d = Settings::self()->touchFriendly() ? new FlickCharmPrivate : 0;
}

FlickCharm::~FlickCharm()
{
    delete d;
}

void FlickCharm::activateOn(QWidget *widget)
{
    if (!d) return;
    QAbstractScrollArea *scrollArea = qobject_cast<QAbstractScrollArea*>(widget);
    if (scrollArea) {
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QWidget *viewport = scrollArea->viewport();

        viewport->installEventFilter(this);
        scrollArea->installEventFilter(this);

        d->flickData.remove(viewport);
        d->flickData[viewport] = new FlickData;
        d->flickData[viewport]->widget = widget;
        d->flickData[viewport]->state = FlickData::Steady;

        return;
    }

    /*
     QWebView *webView = qobject_cast<QWebView*>(widget);
     if (webView) {
         QWebFrame *frame = webView->page()->mainFrame();
         frame->setScrollBarPolicy(Qt::Vertical, Qt::ScrollBarAlwaysOff);
         frame->setScrollBarPolicy(Qt::Horizontal, Qt::ScrollBarAlwaysOff);

         webView->installEventFilter(this);

         d->flickData.remove(webView);
         d->flickData[webView] = new FlickData;
         d->flickData[webView]->widget = webView;
         d->flickData[webView]->state = FlickData::Steady;

         return;
     }
     */
//    qWarning() << "FlickCharm only works on QAbstractScrollArea (and derived classes)";
    //qWarning() << "or QWebView (and derived classes)";
}

void FlickCharm::deactivateFrom(QWidget *widget)
{
    if (!d) return;
    QAbstractScrollArea *scrollArea = qobject_cast<QAbstractScrollArea*>(widget);
    if (scrollArea) {
        QWidget *viewport = scrollArea->viewport();

        viewport->removeEventFilter(this);
        scrollArea->removeEventFilter(this);

        delete d->flickData[viewport];
        d->flickData.remove(viewport);

        return;
    }

    /*
     QWebView *webView = qobject_cast<QWebView*>(widget);
     if (webView) {
         webView->removeEventFilter(this);

         delete d->flickData[webView];
         d->flickData.remove(webView);

         return;
     }
     */
}

static QPoint deaccelerate(const QPoint &speed, const int deltatime)
{
    const int deltaSpeed = deltatime;

    int x = speed.x();
    int y = speed.y();
    x = (x == 0) ? x : (x > 0) ? qMax(0, x - deltaSpeed) : qMin(0, x + deltaSpeed);
    y = (y == 0) ? y : (y > 0) ? qMax(0, y - deltaSpeed) : qMin(0, y + deltaSpeed);
    return QPoint(x, y);
}

bool FlickCharm::eventFilter(QObject *object, QEvent *event)
{
    if (!d || !object->isWidgetType())
        return false;

    const QEvent::Type type = event->type();

    switch (type) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseMove:
    case QEvent::MouseButtonRelease:
        break;
    case QEvent::MouseButtonDblClick: // skip double click
        return true;
    default:
        return false;
    }

    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (type == QEvent::MouseMove && mouseEvent->buttons() != Qt::LeftButton)
        return false;

    if (mouseEvent->modifiers() != Qt::NoModifier)
        return false;

    QWidget *viewport = qobject_cast<QWidget*>(object);
    FlickData *data = d->flickData.value(viewport);
    if (!viewport || !data || data->ignored.removeAll(event))
        return false;

    const QPoint mousePos = mouseEvent->pos();
    //bool consumed = false;
    switch (data->state) {

    case FlickData::Steady:
        if (type == QEvent::MouseButtonPress) {
            //consumed = true;
            data->pressPos = mousePos;
        } else if (type == QEvent::MouseButtonRelease) {
            //consumed = true;
            QMouseEvent *event1 = new QMouseEvent(QEvent::MouseButtonPress,
                                                  data->pressPos, Qt::LeftButton,
                                                  Qt::LeftButton, Qt::NoModifier);
            QMouseEvent *event2 = new QMouseEvent(QEvent::MouseButtonRelease,
                                                  data->pressPos, Qt::LeftButton,
                                                  Qt::LeftButton, Qt::NoModifier);

            data->ignored << event1;
            data->ignored << event2;
            QApplication::postEvent(object, event1);
            QApplication::postEvent(object, event2);
        } else if (type == QEvent::MouseMove) {
            //consumed = true;
            data->scrollTo(mousePos);

            const QPoint delta = mousePos - data->pressPos;
            if (delta.x() > fingerAccuracyThreshold || delta.y() > fingerAccuracyThreshold)
                data->state = FlickData::ManualScroll;
        }
        break;

    case FlickData::ManualScroll:
        if (type == QEvent::MouseMove) {
            //consumed = true;
            data->scrollTo(mousePos);
        } else if (type == QEvent::MouseButtonRelease) {
            //consumed = true;
            data->state = FlickData::AutoScroll;
            data->lastPosValid = false;
            d->startTicker(this);
        }
        break;

    case FlickData::AutoScroll:
        if (type == QEvent::MouseButtonPress) {
            //consumed = true;
            data->state = FlickData::AutoScrollAcceleration;
            data->waitingAcceleration = true;
            data->accelerationTimer.start();
            data->updateSpeed(mousePos);
            data->pressPos = mousePos;
        } else if (type == QEvent::MouseButtonRelease) {
            //consumed = true;
            data->state = FlickData::Steady;
            data->resetSpeed();
        }
        break;

    case FlickData::AutoScrollAcceleration:
        if (type == QEvent::MouseMove) {
            //consumed = true;
            data->updateSpeed(mousePos);
            data->accelerationTimer.start();
            if (data->speed.isNull())
                data->state = FlickData::ManualScroll;
        } else if (type == QEvent::MouseButtonRelease) {
            //consumed = true;
            data->state = FlickData::AutoScroll;
            data->waitingAcceleration = false;
            data->lastPosValid = false;
        }
        break;
    default:
        break;
    }
    data->lastPos = mousePos;
    return true;
}

void FlickCharm::timerEvent(QTimerEvent *event)
{
    int count = 0;
    QHashIterator<QWidget*, FlickData*> item(d->flickData);
    while (item.hasNext()) {
        item.next();
        FlickData *data = item.value();
        if (data->state == FlickData::AutoScrollAcceleration
                && data->waitingAcceleration
                && data->accelerationTimer.elapsed() > 40) {
            data->state = FlickData::ManualScroll;
            data->resetSpeed();
        }
        if (data->state == FlickData::AutoScroll || data->state == FlickData::AutoScrollAcceleration) {
            const int timeElapsed = d->timeCounter.elapsed();
            const QPoint delta = (data->speed) * timeElapsed / 1000;
            bool hasScrolled = data->scrollWidget(delta.x(), delta.y());

            if (data->speed.isNull() || !hasScrolled)
                data->state = FlickData::Steady;
            else
                count++;
            data->speed = deaccelerate(data->speed, timeElapsed);
        }
    }

    if (!count)
        d->ticker.stop();
    else
        d->timeCounter.start();

    QObject::timerEvent(event);
}
