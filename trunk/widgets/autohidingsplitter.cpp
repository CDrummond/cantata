/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 * This file (c) 2012 Piotr Wicijowski <piotr.wicijowski@gmail.com>
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

#include "autohidingsplitter.h"
#include <QSplitterHandle>
#include <QTimer>
#include <QChildEvent>
#include <QResizeEvent>
#include <QComboBox>
#include <QMenu>

class SplitterSizeAnimation:public QVariantAnimation
{
public:
    SplitterSizeAnimation(QObject *parent)
        : QVariantAnimation(parent)
        , splitter(0)
    {
    }
    void setSplitter(QSplitter *splitter) {
        this->splitter = splitter;
    }

protected:
    virtual QVariant interpolated(const QVariant &from, const QVariant &to, qreal progress) const {
        QList<int> fromInt = from.value<QList<int> >();
        QList<int> toInt = to.value<QList<int> >();
        QList<int> returnValue;
        for (int i = 0; i < fromInt.count() ; ++i) {
            returnValue.append((int)((progress)*toInt.at(i) + (1-progress)*fromInt.at(i)));
        }
        return QVariant::fromValue(returnValue);
    }
    virtual void updateCurrentValue(const QVariant &value) {
        if (splitter) {
            splitter->setSizes(value.value<QList<int> >());
        }
    }

private:
    QSplitter *splitter;
};

AutohidingSplitter::AutohidingSplitter(Qt::Orientation orientation, QWidget *parent)
    : QSplitter(orientation, parent)
    , autoHideEnabled(false)
{
    haltModifications = false;
    autohideAnimation = new SplitterSizeAnimation(this);
    autohideAnimation->setSplitter(this);
    autohideAnimation->setDuration(100);
    autohideAnimation->setEasingCurve(QEasingCurve::Linear);
    connect(this, SIGNAL(splitterMoved(int, int)), this, SLOT(updateAfterSplitterMoved(int, int)));
    setMinimumWidth(32);
}

AutohidingSplitter::AutohidingSplitter(QWidget *parent)
    : QSplitter(parent)
    , autoHideEnabled(false)
{
    haltModifications = false;
    autohideAnimation = new SplitterSizeAnimation(this);
    autohideAnimation->setSplitter(this);
    autohideAnimation->setDuration(100);
    autohideAnimation->setEasingCurve(QEasingCurve::Linear);
    connect(this, SIGNAL(splitterMoved(int, int)), this, SLOT(updateAfterSplitterMoved(int, int)));
    setMinimumWidth(32);
}

AutohidingSplitter::~AutohidingSplitter()
{
    foreach(QTimer *sat, animationDelayTimer) {
        sat->stop();
        delete sat;
    }
    animationDelayTimer.clear();
    autohideAnimation->stop();
    delete autohideAnimation;
//    setSizes(expandedSizes);
}

QSplitterHandle * AutohidingSplitter::createHandle()
{
    AutohidingSplitterHandle *sh = new AutohidingSplitterHandle(this->orientation(),this);
    connect(sh, SIGNAL(hoverStarted()), this, SLOT(handleHoverStarted()));
    connect(sh, SIGNAL(hoverFinished()), this, SLOT(handleHoverFinished()));
    return sh;
}

void AutohidingSplitter::addChild(QObject *pObject)
{
    if (pObject && pObject->isWidgetType()) {
        pObject->installEventFilter(this);

        const QObjectList &childList = pObject->children();
        foreach (QObject *obj, childList) {
            addChild(obj);
        }
    }
    QComboBox * combo = qobject_cast<QComboBox *>(pObject);
    if(combo){
        popupsBlockingAutohiding.insert(combo->view());
//        addChild(combo->view());
    }
}

void AutohidingSplitter::removeChild(QObject *pObject)
{
    if (pObject && (pObject->isWidgetType())) {
        pObject->removeEventFilter(this);
        const QObjectList& childList = pObject->children();
        foreach (QObject *obj, childList) {
            removeChild(obj);
        }
    }
}

void AutohidingSplitter::childEvent(QChildEvent *e)
{
//     if (!autoHideEnabled) {
//         QSplitter::childEvent(e);
//         return;
//     }
    if (e->child()->isWidgetType()) {
        if (QEvent::ChildAdded==e->type()) {
            addChild(e->child());
        } else if (QEvent::ChildRemoved==e->type()) {
            removeChild(e->child());
        }
    }

    QSplitter::childEvent(e);
}

bool AutohidingSplitter::eventFilter(QObject *target, QEvent *e)
{
    switch (e->type()) {
    case QEvent::ChildAdded: {
        QChildEvent *ce = (QChildEvent*)e;
        addChild(ce->child());
        break;
    }
    case QEvent::ChildRemoved: {
        QChildEvent *ce = (QChildEvent*)e;
        removeChild(ce->child());
        break;
    }
    default:
        break;
    }
    if (!autoHideEnabled) {
        return QSplitter::eventFilter(target, e);
    }
    switch (e->type()) {
    case QEvent::Enter:
        this->widgetHoverStarted(indexOf(qobject_cast<QWidget *>(target)));
        if(popupsBlockingAutohiding.contains(qobject_cast<QWidget *>(target))) {
            haltModifications = true;
        }
        break;
    case QEvent::Leave:
        this->widgetHoverFinished(indexOf(qobject_cast<QWidget *>(target)));
        if(popupsBlockingAutohiding.contains(qobject_cast<QWidget *>(target))) {
            haltModifications = false;
        }
        break;
    case QEvent::MouseButtonPress:
        if(qobject_cast<AutohidingSplitterHandle *>(target)){
            foreach(QTimer * timer, animationDelayTimer) {
                timer->stop();
            }
            haltModifications = true;
        }
        break;
    case QEvent::MouseButtonRelease:{
        if(qobject_cast<AutohidingSplitterHandle *>(target)){
            haltModifications = false;
            targetSizes.clear();
        }
        break;
    }
    case QEvent::FocusIn:{
//     QFocusEvent *ce = (QFocusEvent *)e;
        haltModifications = false;
        break;
    }
    case QEvent::FocusOut:{
//     QFocusEvent *ce = (QFocusEvent *)e;
        haltModifications = true;
        break;
    }
    default:
        break;
    }

    return QSplitter::eventFilter(target, e);
}

void AutohidingSplitter::setAutoHideEnabled(bool ah)
{
    if (ah==autoHideEnabled) {
        return;
    }

    autoHideEnabled=ah;
    if (autoHideEnabled) {
        expandedSizes = sizes();
        connect(this, SIGNAL(splitterMoved(int, int)), this, SLOT(updateAfterSplitterMoved(int, int)));
    } else {
        for(int i = 0; i < widgetAutohidden.count() ; ++i) {
            widgetAutohidden[i]=false;
        }
        disconnect(this, SIGNAL(splitterMoved(int, int)), this, SLOT(updateAfterSplitterMoved(int, int)));
    }
}

void AutohidingSplitter::setVisible(bool visible){
    haltModifications=!visible;
    QSplitter::setVisible(visible);
}

void AutohidingSplitter::resizeEvent(QResizeEvent *event)
{
    if (autoHideEnabled && !haltModifications && event->oldSize().width() > 0) {
//        int oldUsableSize = event->oldSize().width()/*-(count()-1)*handleWidth()*/;
        int oldUsableSize = 0;
        foreach(int size, expandedSizes)
            oldUsableSize+=size;
        int newUsableSize = event->size().width()/*-(count()-1)*handleWidth()*/;
        int leftToDistribute = newUsableSize-oldUsableSize;

        for (int i = 0; i < count()-1; ++ i) {
            expandedSizes[i]+=int(qreal(leftToDistribute)/(count()-i));
            leftToDistribute-=int(qreal(leftToDistribute)/(count()-i));
        }
        if (count()>0) {
            expandedSizes[count()-1]+=leftToDistribute;
        }
    }
    QSplitter::resizeEvent(event);

}

void AutohidingSplitter::addWidget(QWidget *widget)
{
    QSplitter::addWidget(widget);
    expandedSizes.append(widget->size().width());
    if (count()+1!=widgetAutohidable.count()) {
        QTimer *sat = new QTimer(this);
        sat->setSingleShot(true);
        sat->setInterval(500);
        connect(sat, SIGNAL(timeout()), this, SLOT(setWidgetForHiding()));
        animationDelayTimer.append(sat);
        widgetAutohidden.append(false);
        widgetAutohiddenPrev.append(false);
        widgetAutohidable.append(false);
    }
}

void AutohidingSplitter::setAutohidable(int index, bool autohidable) {
    widgetAutohidable[index]=autohidable;
    widgetAutohidden[index]=autohidable;
//     updateResizeQueue();
    setSizes(getSizesAfterHiding());
}

bool AutohidingSplitter::restoreState(const QByteArray &state)
{
    bool result = QSplitter::restoreState(state);
    expandedSizes = sizes();
    return result;
}

QByteArray AutohidingSplitter::saveState() const
{
    if (!autoHideEnabled) {
        return QSplitter::saveState();
    }
    AutohidingSplitter *tmpSplitter = new AutohidingSplitter(/*qobject_cast<QWidget *>(parent())*/);

    for (int i = 0; i < count(); ++ i) {
        QWidget *widget = new QWidget(/*tmpSplitter*/);
        tmpSplitter->addWidget(widget);
    }
    tmpSplitter->setSizes(expandedSizes);
    QByteArray result = tmpSplitter->QSplitter::saveState();
    delete tmpSplitter;
    return result;
}

void AutohidingSplitter::widgetHoverStarted(int index)
{
    if (!autoHideEnabled || index<0 || index > count()) {
        return;
    }
    if(!haltModifications){
        if (animationDelayTimer.at(index)->isActive()) {
            animationDelayTimer.at(index)->stop();
        }
        if (widgetAutohidden.at(index) && widgetAutohidable.at(index)) {
            widgetAutohiddenPrev[index] = widgetAutohidden[index];
            widgetAutohidden[index] = false;
            updateResizeQueue();
        }
    }
}

void AutohidingSplitter::widgetHoverFinished(int index)
{
    if (!autoHideEnabled || index<0 || index > count()) {
        return;
    }
    if (!widgetAutohidden.at(index) && widgetAutohidable.at(index)) {
        animationDelayTimer.at(index)->start();
    }
}

void AutohidingSplitter::handleHoverStarted()
{
    if (!autoHideEnabled) {
        return;
    }

    if(!haltModifications){
        int index = indexOf(qobject_cast<QWidget *>(QObject::sender()));
        if (animationDelayTimer.at(index)->isActive()) {
            animationDelayTimer.at(index)->stop();
        }
        if (widgetAutohidden.at(index) && widgetAutohidable.at(index)) {
            widgetAutohiddenPrev[index] = widgetAutohidden[index];
            widgetAutohidden[index] = false;
            updateResizeQueue();
        }
        if (index > 0 && animationDelayTimer.at(index-1)->isActive()) {
            animationDelayTimer.at(index-1)->stop();
        }
        if (index > 0 && widgetAutohidden.at(index-1) && widgetAutohidable.at(index-1)) {
            widgetAutohiddenPrev[index-1] = widgetAutohidden[index-1];
            widgetAutohidden[index-1] = false;
            updateResizeQueue();
        }
    }
}

void AutohidingSplitter::handleHoverFinished()
{
    if (!autoHideEnabled) {
        return;
    }

    if(!haltModifications){
        int index = indexOf(qobject_cast<QWidget *>(QObject::sender()));
        if (!widgetAutohidden.at(index) && widgetAutohidable.at(index)) {
            animationDelayTimer.at(index)->start();
        }
        if (index>0 && !widgetAutohidden.at(index-1) && widgetAutohidable.at(index-1)) {
            animationDelayTimer.at(index-1)->start();
        }
    }
}

void AutohidingSplitter::updateResizeQueue()
{
    if (!autoHideEnabled) {
        return;
    }

    targetSizes.enqueue(getSizesAfterHiding());
    if (autohideAnimation->state()==QAbstractAnimation::Stopped) {
        startAnimation();
    }
}

QList<int> AutohidingSplitter::getSizesAfterHiding() const
{
    int toDistribute = 0;
    int numberOfExpanded = 0;
    QList<int> result;
    result = sizes();
    for (int i = 0 ; i<widgetAutohidden.count(); ++i) {
        if (widgetAutohidable.at(i)) {
            if (widgetAutohidden.at(i)) {
                toDistribute+=result.at(i);
                result[i]=0;
            } else {
                toDistribute+=result.at(i)-expandedSizes.at(i);
                result[i]=expandedSizes.at(i);
                if(widgetAutohidden.at(i)==widgetAutohiddenPrev.at(i))
                    numberOfExpanded++;
            }
        } else {
            toDistribute+=result.at(i)-expandedSizes.at(i);
            result[i]=expandedSizes.at(i);
            numberOfExpanded++;
        }
    }
    for (int i = 0 ; i<widgetAutohidden.count(); ++i) {
        if( (widgetAutohidable.at(i)&&!widgetAutohidden.at(i)&&widgetAutohidden.at(i)==widgetAutohiddenPrev.at(i)) || !widgetAutohidable.at(i)) {
                numberOfExpanded--;
                if (numberOfExpanded) {
                    result[i]+=int(qreal(toDistribute)/(numberOfExpanded+1));
                    toDistribute-=int(qreal(toDistribute)/(numberOfExpanded+1));
                } else {
                    result[i]+=toDistribute;
                }
        }
    }
    return result;
}

void AutohidingSplitter::startAnimation()
{
    if (!autoHideEnabled) {
        return;
    }

    disconnect(this,SLOT(startAnimation()));
    if (!targetSizes.isEmpty()) {
        QList<int> nextSizes = targetSizes.dequeue();

        autohideAnimation->setStartValue(QVariant::fromValue(this->sizes()));
        autohideAnimation->setCurrentTime(0);
        autohideAnimation->setEndValue(QVariant::fromValue(nextSizes));
        connect(autohideAnimation,SIGNAL(finished()),this,SLOT(startAnimation()));
        autohideAnimation->start();
    }
}

void AutohidingSplitter::setWidgetForHiding()
{

    int index = animationDelayTimer.indexOf(qobject_cast<QTimer *>(QObject::sender()));
    if(!haltModifications){
        if (!widgetAutohidden.at(index)) {
            widgetAutohiddenPrev[index] = widgetAutohidden[index];
            widgetAutohidden[index] = true;
            updateResizeQueue();
        }
    }
}

void AutohidingSplitter::updateAfterSplitterMoved(int pos, int index)
{
    Q_UNUSED(pos);
    if (!autoHideEnabled || index<=0 || index>count()) {
        return;
    }
    QList<int> currentTemporarySizes = sizes();
    QList<int> previousTemporarySizes = getSizesAfterHiding();
    expandedSizes[index-1]+=currentTemporarySizes.at(index-1)-previousTemporarySizes.at(index-1);
    expandedSizes[index]+=currentTemporarySizes.at(index)-previousTemporarySizes.at(index);
}
