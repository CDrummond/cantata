/**
  * This file is part of the KDE project
  * Copyright (C) 2007, 2009 Rafael Fernández López <ereslibre@kde.org>
  *
  * This library is free software; you can redistribute it and/or
  * modify it under the terms of the GNU Library General Public
  * License as published by the Free Software Foundation; either
  * version 2 of the License, or (at your option) any later version.
  *
  * This library is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  * Library General Public License for more details.
  *
  * You should have received a copy of the GNU Library General Public License
  * along with this library; see the file COPYING.LIB.  If not, write to
  * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
  * Boston, MA 02110-1301, USA.
  */

#ifndef KCATEGORYDRAWER_H
#define KCATEGORYDRAWER_H

//#include <kitemviews_export.h>

#include <QtCore/QObject>
#include <QMouseEvent>
class KCategoryDrawerPrivate;

class QPainter;
class QModelIndex;
class QStyleOption;

class KCategorizedView;

/**
  * @class KCategoryDrawer kcategorydrawer.h KCategoryDrawer
  *
  * The category drawing is performed by this class. It also gives information about the category
  * height and margins.
  *
  */
class /*KITEMVIEWS_EXPORT*/ KCategoryDrawer
    : public QObject
{
    Q_OBJECT
    friend class KCategorizedView;

public:
    /*
     * Construct a category drawer for a given view
     *
     * @since 5.0
     */
    KCategoryDrawer(KCategorizedView *view);
    virtual ~KCategoryDrawer();

    /**
      * @return The view this category drawer is associated with.
      */
    KCategorizedView *view() const;

    /**
      * This method purpose is to draw a category represented by the given
      * @param index with the given @param sortRole sorting role
      *
      * @note This method will be called one time per category, always with the
      *       first element in that category
      */
    virtual void drawCategory(const QModelIndex &index,
                              int sortRole,
                              const QStyleOption &option,
                              QPainter *painter) const;

    /**
      * @return The category height for the category representated by index @p index with
      *         style options @p option.
      */
    virtual int categoryHeight(const QModelIndex &index, const QStyleOption &option) const;

    /**
      * @note 0 by default
      *
      * @since 4.4
      */
    virtual int leftMargin() const;

    /**
      * @note 0 by default
      *
      * @since 4.4
      */
    virtual int rightMargin() const;

Q_SIGNALS:
    /**
      * This signal becomes emitted when collapse or expand has been clicked.
      */
    void collapseOrExpandClicked(const QModelIndex &index);

    /**
      * Emit this signal on your subclass implementation to notify that something happened. Usually
      * this will be triggered when you have received an event, and its position matched some "hot spot".
      *
      * You give this action the integer you want, and having connected this signal to your code,
      * the connected slot can perform the needed changes (view, model, selection model, delegate...)
      */
    void actionRequested(int action, const QModelIndex &index);

protected:
    /**
      * Method called when the mouse button has been pressed.
      *
      * @param index The representative index of the block of items.
      * @param blockRect The rect occupied by the block of items.
      * @param event The mouse event.
      *
      * @warning You explicitly have to determine whether the event has been accepted or not. You
      *          have to call event->accept() or event->ignore() at all possible case branches in
      *          your code.
      */
    virtual void mouseButtonPressed(const QModelIndex &index, const QRect &blockRect, QMouseEvent *event);

    /**
      * Method called when the mouse button has been released.
      *
      * @param index The representative index of the block of items.
      * @param blockRect The rect occupied by the block of items.
      * @param event The mouse event.
      *
      * @warning You explicitly have to determine whether the event has been accepted or not. You
      *          have to call event->accept() or event->ignore() at all possible case branches in
      *          your code.
      */
    virtual void mouseButtonReleased(const QModelIndex &index, const QRect &blockRect, QMouseEvent *event);

    /**
      * Method called when the mouse has been moved.
      *
      * @param index The representative index of the block of items.
      * @param blockRect The rect occupied by the block of items.
      * @param event The mouse event.
      */
    virtual void mouseMoved(const QModelIndex &index, const QRect &blockRect, QMouseEvent *event);

    /**
      * Method called when the mouse button has been double clicked.
      *
      * @param index The representative index of the block of items.
      * @param blockRect The rect occupied by the block of items.
      * @param event The mouse event.
      *
      * @warning You explicitly have to determine whether the event has been accepted or not. You
      *          have to call event->accept() or event->ignore() at all possible case branches in
      *          your code.
      */
    virtual void mouseButtonDoubleClicked(const QModelIndex &index, const QRect &blockRect, QMouseEvent *event);

    /**
      * Method called when the mouse button has left this block.
      *
      * @param index The representative index of the block of items.
      * @param blockRect The rect occupied by the block of items.
      */
    virtual void mouseLeft(const QModelIndex &index, const QRect &blockRect);

private:
    KCategoryDrawerPrivate *const d;
};

#ifndef KITEMVIEWS_NO_DEPRECATED
/**
 * @class KCategoryDrawerV3 kcategorydrawer.h KCategoryDrawer
 *
 * @deprecated Use KCategoryDrawer instead
 */
/*
class KCategoryDrawerV3 : public KCategoryDrawer
{
public:
    KITEMVIEWS_DEPRECATED KCategoryDrawerV3(KCategorizedView *view) : KCategoryDrawer(view) {}
};
*/
#endif

#endif // KCATEGORYDRAWER_H
