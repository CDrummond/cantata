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

#ifndef DYNAMIC_H
#define DYNAMIC_H

#include <QtCore/QAbstractItemModel>
#include <QtCore/QList>
#include <QtCore/QMap>
#include <QtCore/QString>

class QTimer;

class Dynamic : public QAbstractItemModel
{
    Q_OBJECT

public:
    typedef QMap<QString, QString> Rule;
    struct Entry {
        QString name;
        QList<Rule> rules;
    };

    static Dynamic * self();

    Dynamic();

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex&) const { return 1; }
    bool hasChildren(const QModelIndex &parent) const;
    QModelIndex parent(const QModelIndex &index) const;
    QModelIndex index(int row, int column, const QModelIndex &parent) const;
    QVariant data(const QModelIndex &, int) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    Entry entry(const QString &e);
    bool exists(const QString &e) {
        return entryList.end()!=find(e);
    }
    bool save(const Entry &e);
    bool del(const QString &name);
    bool start(const QString &name);
    bool stop();
    bool isRunning();
    QString current() const {
        return currentEntry;
    }
    const QList<Entry> & entries() const {
        return entryList;
    }

Q_SIGNALS:
    void running(bool status);
    void error(const QString &str);

    // These are for communicating with MPD object (which is in its own thread, so need to talk via signal/slots)
    void clear();

private Q_SLOTS:
    void checkHelper();

private:
    int getPid() const;
    bool controlApp(bool isStart);
    QList<Entry>::Iterator find(const QString &e);

private:
    QTimer *timer;
    QList<Entry> entryList;
    QString currentEntry;
};

#endif
