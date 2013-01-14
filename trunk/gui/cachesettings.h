/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/* This file is part of Clementine.
   Copyright 2010, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef CACHESETTINGS_H
#define CACHESETTINGS_H

#include <QtGui/QWidget>
#include <QtCore/QString>
#include <QtCore/QStringList>
#include <QtGui/QGroupBox>

class QThread;
class CacheItemCounter : public QObject
{
    Q_OBJECT

public:
    CacheItemCounter(const QString &d, const QStringList &t);
    ~CacheItemCounter();

Q_SIGNALS:
    void count(int num, int space);

public Q_SLOTS:
    void getCount();
    void deleteAll();

private:
    QString dir;
    QStringList types;
    QThread *thread;
};

class QLabel;
class QPushButton;
class CacheItem : public QGroupBox
{
    Q_OBJECT

public:
    CacheItem(const QString &title, const QString &d, const QStringList &t, QWidget *parent);
    ~CacheItem();

    void showEvent(QShowEvent *e);

Q_SIGNALS:
    void getCount();
    void deleteAll();

private Q_SLOTS:
    void deleteAllItems();
    void update(int itemCount, int space);

private:
    bool calculated;
    CacheItemCounter *counter;
    QPushButton *button;
    QLabel *numItems;
    QLabel *spaceUsed;
};

class CacheSettings : public QWidget
{
    Q_OBJECT

public:
    CacheSettings(QWidget *parent);
    ~CacheSettings();
};

#endif
