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

#ifndef AUTOHIDINGSPLITTER_H
#define AUTOHIDINGSPLITTER_H

#include "config.h"
#include <QtGui/QSplitter>

#include <QtCore/QList>
#include <QtCore/QQueue>
#include <QtCore/QVariantAnimation>
#include <QtCore/QSet>
#include <QtGui/QAbstractItemView>

Q_DECLARE_METATYPE(QList<int>)

class SplitterSizeAnimation;

class AutohidingSplitterHandle : public QSplitterHandle
{
    Q_OBJECT

public:
    AutohidingSplitterHandle(Qt::Orientation orientation, QSplitter *parent)
        :QSplitterHandle(orientation,parent) {
    }
    virtual ~AutohidingSplitterHandle() {
    }

Q_SIGNALS:
    void hoverStarted();
    void hoverFinished();

protected:
    virtual void enterEvent(QEvent *){
        emit hoverStarted();
    }
    virtual void leaveEvent(QEvent *){
        emit hoverFinished();
    }
};

class AutohidingSplitter : public QSplitter
{
    Q_OBJECT

public:
    explicit AutohidingSplitter(Qt::Orientation orientation, QWidget *parent=0);
    explicit AutohidingSplitter(QWidget *parent=0);
    virtual ~AutohidingSplitter();

    void setAutohidable(int index, bool autohidable = true);
    void addWidget(QWidget *widget);
    bool restoreState( const QByteArray &state);
    QByteArray saveState() const;
    bool eventFilter(QObject *watched, QEvent *event);
    bool isAutoHideEnabled() const {
        return autoHideEnabled;
    }

public Q_SLOTS:
    void setAutoHideEnabled(bool en);
    void setVisible(bool visible);

protected:
    virtual QSplitterHandle * createHandle();
    void childEvent(QChildEvent *);
    void removeChild(QObject* pObject);
    void addChild(QObject *pObject);
    void resizeEvent(QResizeEvent *);

private Q_SLOTS:
    void widgetHoverStarted(int index);
    void widgetHoverFinished(int index);
    void handleHoverStarted();
    void handleHoverFinished();
    void updateResizeQueue();
    void setWidgetForHiding();
    void startAnimation();
    void updateAfterSplitterMoved(int pos, int index);
    void inhibitModifications(){haltModifications = true;}
    void resumeModifications(){haltModifications = false;}

private:
    bool autoHideEnabled;
    bool haltModifications;
    QList<int> getSizesAfterHiding()const;
    SplitterSizeAnimation *autohideAnimation;
    QList<QTimer *> animationDelayTimer;
    QList<bool> widgetAutohidden;
    QList<bool> widgetAutohiddenPrev;
    QList<bool> widgetAutohidable;
    QList<int> expandedSizes;
    QQueue<QList<int> > targetSizes;
    QSet<QWidget *> popupsBlockingAutohiding;
    friend class AutohidingSplitterHandle;
};

#endif
