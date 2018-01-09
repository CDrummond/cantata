/***************************************************************************
 *   Copyright (C) 2005-2015 by the Quassel Project                        *
 *   devel@quassel-irc.org                                                 *
 *                                                                         *
 *   This class has been inspired by KDE's KKeySequenceWidget and uses     *
 *   some code snippets of its implementation, part of kdelibs.            *
 *   The original file is                                                  *
 *       Copyright (C) 1998 Mark Donohoe <donohoe@kde.org>                 *
 *       Copyright (C) 2001 Ellis Whitehead <ellis@kde.org>                *
 *       Copyright (C) 2007 Andreas Hartmetz <ahartmetz@gmail.com>         *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.         *
 ***************************************************************************/

#ifndef KEYSEQUENCEWIDGET_H
#define KEYSEQUENCEWIDGET_H

#include <QKeySequence>
#include <QPushButton>
#include <QSet>
#include <QWidget>

#include "shortcutsmodel.h"

class Action;
class ActionCollection;
class KeySequenceButton;
class QToolButton;

class KeySequenceWidget : public QWidget
{
    Q_OBJECT
public:
    KeySequenceWidget(QWidget *parent = nullptr);

    void setModel(ShortcutsModel *model);

public slots:
    void setKeySequence(const QKeySequence &seq);

signals:
    /**
     * This signal is emitted when the current key sequence has changed by user input
     * \param seq         The key sequence the user has chosen
     * \param conflicting The index of an action that needs to have its shortcut removed. The user has already been
     *                    asked to agree (if he declines, this signal won't be emitted at all).
     */
    void keySequenceChanged(const QKeySequence &seq, const QModelIndex &conflicting = QModelIndex());

    void clicked();

private slots:
    void updateShortcutDisplay();
    void startRecording();
    void cancelRecording();
    void clear();

private:
    inline bool isRecording() const { return _isRecording; }
    void doneRecording();

    bool isOkWhenModifierless(int keyQt) const;
    bool isShiftAsModifierAllowed(int keyQt) const;
    bool isKeySequenceAvailable(const QKeySequence &seq);

    ShortcutsModel *_shortcutsModel;
    bool _isRecording;
    QKeySequence _keySequence, _oldKeySequence;
    uint _modifierKeys;
    QModelIndex _conflictingIndex;

    KeySequenceButton *_keyButton;
    QToolButton *_clearButton;

    friend class KeySequenceButton;
};


/*****************************************************************************/

class KeySequenceButton : public QPushButton
{
    Q_OBJECT
public:
    explicit KeySequenceButton(KeySequenceWidget *d, QWidget *parent = nullptr);

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    KeySequenceWidget *d;
};


#endif // KEYSEQUENCEWIDGET_H
