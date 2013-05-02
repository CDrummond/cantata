/***************************************************************************
 *   Copyright (C) 2010 by the Quassel Project                             *
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
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <QApplication>
#include <QKeyEvent>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QToolButton>

// This defines the unicode symbols for special keys (kCommandUnicode and friends)
#ifdef Q_WS_MAC
#  include <Carbon/Carbon.h>
#endif

#include "icon.h"
#include "action.h"
#include "actioncollection.h"
#include "keysequencewidget.h"

KeySequenceButton::KeySequenceButton(KeySequenceWidget *d_, QWidget *parent)
  : QPushButton(parent),
  d(d_)
{

}

bool KeySequenceButton::event(QEvent *e) {
  if(d->isRecording() && e->type() == QEvent::KeyPress) {
    keyPressEvent(static_cast<QKeyEvent *>(e));
    return true;
  }

  // The shortcut 'alt+c' ( or any other dialog local action shortcut )
  // ended the recording and triggered the action associated with the
  // action. In case of 'alt+c' ending the dialog.  It seems that those
  // ShortcutOverride events get sent even if grabKeyboard() is active.
  if(d->isRecording() && e->type() == QEvent::ShortcutOverride) {
    e->accept();
    return true;
  }

  return QPushButton::event(e);
}

void KeySequenceButton::keyPressEvent(QKeyEvent *e) {
  int keyQt = e->key();
  if(keyQt == -1) {
    // Qt sometimes returns garbage keycodes, I observed -1, if it doesn't know a key.
    // We cannot do anything useful with those (several keys have -1, indistinguishable)
    // and QKeySequence.toString() will also yield a garbage string.
    QMessageBox::information(this,
                             tr("The key you just pressed is not supported by Qt."),
                             tr("Unsupported Key"));
    return d->cancelRecording();
  }

  uint newModifiers = e->modifiers() & (Qt::SHIFT | Qt::CTRL | Qt::ALT | Qt::META);

  //don't have the return or space key appear as first key of the sequence when they
  //were pressed to start editing - catch and them and imitate their effect
  if(!d->isRecording() && ((keyQt == Qt::Key_Return || keyQt == Qt::Key_Space))) {
    d->startRecording();
    d->_modifierKeys = newModifiers;
    d->updateShortcutDisplay();
    return;
  }

  // We get events even if recording isn't active.
  if(!d->isRecording())
    return QPushButton::keyPressEvent(e);

  e->accept();
  d->_modifierKeys = newModifiers;

  switch(keyQt) {
  case Qt::Key_AltGr: //or else we get unicode salad
    return;
  case Qt::Key_Shift:
  case Qt::Key_Control:
  case Qt::Key_Alt:
  case Qt::Key_Meta:
  case Qt::Key_Menu: //unused (yes, but why?)
    d->updateShortcutDisplay();
    break;

  default:
    if(!(d->_modifierKeys & ~Qt::SHIFT)) {
      // It's the first key and no modifier pressed. Check if this is
      // allowed
      if(!d->isOkWhenModifierless(keyQt))
        return;
    }

    // We now have a valid key press.
    if(keyQt) {
      if((keyQt == Qt::Key_Backtab) && (d->_modifierKeys & Qt::SHIFT)) {
        keyQt = Qt::Key_Tab | d->_modifierKeys;
      }
      else if(d->isShiftAsModifierAllowed(keyQt)) {
        keyQt |= d->_modifierKeys;
      } else
        keyQt |= (d->_modifierKeys & ~Qt::SHIFT);

      d->_keySequence = QKeySequence(keyQt);
      d->doneRecording();
    }
  }
}

void KeySequenceButton::keyReleaseEvent(QKeyEvent *e) {
  if(e->key() == -1) {
    // ignore garbage, see keyPressEvent()
    return;
  }

  if(!d->isRecording())
    return QPushButton::keyReleaseEvent(e);

  e->accept();

  uint newModifiers = e->modifiers() & (Qt::SHIFT | Qt::CTRL | Qt::ALT | Qt::META);

  // if a modifier that belongs to the shortcut was released...
  if((newModifiers & d->_modifierKeys) < d->_modifierKeys) {
    d->_modifierKeys = newModifiers;
    d->updateShortcutDisplay();
  }
}

/******************************************************************************/

KeySequenceWidget::KeySequenceWidget(QWidget *parent)
  : QWidget(parent),
  _shortcutsModel(0),
  _isRecording(false),
  _modifierKeys(0)
{
  QHBoxLayout *layout = new QHBoxLayout(this);
  layout->setMargin(0);

  _keyButton = new KeySequenceButton(this, this);
  _keyButton->setFocusPolicy(Qt::StrongFocus);
  //TODO: _keyButton->setIcon(SmallIcon("configure"));
  _keyButton->setToolTip(tr("Click on the button, then enter the shortcut like you would in the program.\nExample for Ctrl+a: hold the Ctrl key and press a."));
  layout->addWidget(_keyButton);

  _clearButton = new QToolButton(this);
  layout->addWidget(_clearButton);

  Icon icon(qApp->isLeftToRight() ? "edit-clear-locationbar-rtl" : "edit-clear-locationbar-ltr");

  if (icon.isNull()) {
      icon=Icon("edit-clear");
  }
  _clearButton->setIcon(icon);

  setLayout(layout);

  connect(_keyButton, SIGNAL(clicked()), SLOT(startRecording()));
  connect(_keyButton, SIGNAL(clicked()), SIGNAL(clicked()));
  connect(_clearButton, SIGNAL(clicked()), SLOT(clear()));
  connect(_clearButton, SIGNAL(clicked()), SIGNAL(clicked()));
}

void KeySequenceWidget::setModel(ShortcutsModel *model) {
  Q_ASSERT(!_shortcutsModel);
  _shortcutsModel = model;
}

bool KeySequenceWidget::isOkWhenModifierless(int keyQt) const {
  //this whole function is a hack, but especially the first line of code
  if(QKeySequence(keyQt).toString().length() == 1)
    return false;

  switch(keyQt) {
  case Qt::Key_Return:
  case Qt::Key_Space:
  case Qt::Key_Tab:
  case Qt::Key_Backtab: //does this ever happen?
  case Qt::Key_Backspace:
  case Qt::Key_Delete:
    return false;
  default:
    return true;
  }
}

bool KeySequenceWidget::isShiftAsModifierAllowed(int keyQt) const {
  // Shift only works as a modifier with certain keys. It's not possible
  // to enter the SHIFT+5 key sequence for me because this is handled as
  // '%' by qt on my keyboard.
  // The working keys are all hardcoded here :-(
  if(keyQt >= Qt::Key_F1 && keyQt <= Qt::Key_F35)
    return true;

  if(QChar(keyQt).isLetter())
    return true;

  switch(keyQt) {
  case Qt::Key_Return:
  case Qt::Key_Space:
  case Qt::Key_Backspace:
  case Qt::Key_Escape:
  case Qt::Key_Print:
  case Qt::Key_ScrollLock:
  case Qt::Key_Pause:
  case Qt::Key_PageUp:
  case Qt::Key_PageDown:
  case Qt::Key_Insert:
  case Qt::Key_Delete:
  case Qt::Key_Home:
  case Qt::Key_End:
  case Qt::Key_Up:
  case Qt::Key_Down:
  case Qt::Key_Left:
  case Qt::Key_Right:
    return true;

  default:
    return false;
  }
}

void KeySequenceWidget::updateShortcutDisplay() {
  QString s = _keySequence.toString(QKeySequence::NativeText);
  s.replace('&', QLatin1String("&&"));

  if(_isRecording) {
    if(_modifierKeys) {
#ifdef Q_WS_MAC
      if(_modifierKeys & Qt::META)  s += QChar(kControlUnicode);
      if(_modifierKeys & Qt::ALT)   s += QChar(kOptionUnicode);
      if(_modifierKeys & Qt::SHIFT) s += QChar(kShiftUnicode);
      if(_modifierKeys & Qt::CTRL)  s += QChar(kCommandUnicode);
#else
      if(_modifierKeys & Qt::META)  s += tr("Meta", "Meta key") + '+';
      if(_modifierKeys & Qt::CTRL)  s += tr("Ctrl", "Ctrl key") + '+';
      if(_modifierKeys & Qt::ALT)   s += tr("Alt", "Alt key") + '+';
      if(_modifierKeys & Qt::SHIFT) s += tr("Shift", "Shift key") + '+';
#endif
    } else {
      s = tr("Input", "What the user inputs now will be taken as the new shortcut");
    }
    // make it clear that input is still going on
    s.append(" ...");
  }

  if(s.isEmpty()) {
    s = tr("None", "No shortcut defined");
  }

  s.prepend(' ');
  s.append(' ');
  _keyButton->setText(s);
}

void KeySequenceWidget::startRecording() {
  _modifierKeys = 0;
  _oldKeySequence = _keySequence;
  _keySequence = QKeySequence();
  _conflictingIndex = QModelIndex();
  _isRecording = true;
  _keyButton->grabKeyboard();

//   if(!QWidget::keyboardGrabber()) {
//     qWarning() << "Failed to grab the keyboard! Most likely qt's nograb option is active";
//   }

  _keyButton->setDown(true);
  updateShortcutDisplay();
}


void KeySequenceWidget::doneRecording() {
  bool wasRecording = _isRecording;
  _isRecording = false;
  _keyButton->releaseKeyboard();
  _keyButton->setDown(false);

  if(!wasRecording || _keySequence == _oldKeySequence) {
    // The sequence hasn't changed
    updateShortcutDisplay();
    return;
  }

  if(!isKeySequenceAvailable(_keySequence)) {
    _keySequence = _oldKeySequence;
  } else if(wasRecording) {
    emit keySequenceChanged(_keySequence, _conflictingIndex);
  }
  updateShortcutDisplay();
}

void KeySequenceWidget::cancelRecording() {
  _keySequence = _oldKeySequence;
  doneRecording();
}

void KeySequenceWidget::setKeySequence(const QKeySequence &seq) {
  // oldKeySequence holds the key sequence before recording started, if setKeySequence()
  // is called while not recording then set oldKeySequence to the existing sequence so
  // that the keySequenceChanged() signal is emitted if the new and previous key
  // sequences are different
  if(!isRecording())
    _oldKeySequence = _keySequence;

  _keySequence = seq;
  _clearButton->setVisible(!_keySequence.isEmpty());
  doneRecording();
}

void KeySequenceWidget::clear() {
  setKeySequence(QKeySequence());
  // setKeySequence() won't emit a signal when we're not recording
  emit keySequenceChanged(QKeySequence());
}

bool KeySequenceWidget::isKeySequenceAvailable(const QKeySequence &seq) {
  if(seq.isEmpty())
    return true;

  // We need to access the root model, not the filtered one
  for(int cat = 0; cat < _shortcutsModel->rowCount(); cat++) {
    QModelIndex catIdx = _shortcutsModel->index(cat, 0);
    for(int r = 0; r < _shortcutsModel->rowCount(catIdx); r++) {
      QModelIndex actIdx = _shortcutsModel->index(r, 0, catIdx);
      Q_ASSERT(actIdx.isValid());
      if(actIdx.data(ShortcutsModel::ActiveShortcutRole).value<QKeySequence>() != seq)
        continue;

      if(!actIdx.data(ShortcutsModel::IsConfigurableRole).toBool()) {
        QMessageBox::warning(this, tr("Shortcut Conflict"),
                             tr("The \"%1\" shortcut is already in use, and cannot be configured.\nPlease choose another one.").arg(seq.toString(QKeySequence::NativeText)),
                             QMessageBox::Ok);
        return false;
      }

      QMessageBox box(QMessageBox::Warning, tr("Shortcut Conflict"),
                      (tr("The \"%1\" shortcut is ambiguous with the shortcut for the following action:")
                       + "<br><ul><li>%2</li></ul><br>"
                       + tr("Do you want to reassign this shortcut to the selected action?")
                       ).arg(seq.toString(QKeySequence::NativeText), actIdx.data().toString()),
                      QMessageBox::Cancel, this);
      box.addButton(tr("Reassign"), QMessageBox::AcceptRole);
      if(box.exec() == QMessageBox::Cancel)
        return false;

      _conflictingIndex = actIdx;
      return true;
    }
  }
  return true;
}
