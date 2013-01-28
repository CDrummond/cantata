/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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

#ifndef SPINBOX_H
#define SPINBOX_H

#include <QSpinBox>
#include <QFontMetrics>

class EmptySpinBox : public QSpinBox
{
public:
    EmptySpinBox(QWidget *parent)
        : QSpinBox(parent)
        , allowEmpty(false) {
    }

    void setAllowEmpty() {
        setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        setKeyboardTracking(true);
        setMaximum(3000);
        allowEmpty=true;
    }

    QSize sizeHint() const {
        return QSpinBox::sizeHint()+QSize(fontMetrics().height()/2, 0);
    }

protected:
    virtual QValidator::State validate(QString &input, int &pos) const {
        return allowEmpty && input.isEmpty() ? QValidator::Acceptable : QSpinBox::validate(input, pos);
    }

    virtual int valueFromText(const QString &text) const {
        return allowEmpty && text.isEmpty() ? minimum() : QSpinBox::valueFromText(text);
    }

    virtual QString textFromValue(int val) const {
        return allowEmpty && val==minimum() ? QString() : QSpinBox::textFromValue(val);
    }
private:
    bool allowEmpty;
};

#ifdef Q_WS_WIN
typedef EmptySpinBox SpinBox;
#else
class QToolButton;

class SpinBox : public QWidget
{
    Q_OBJECT

public:
    SpinBox(QWidget *p);
    virtual ~SpinBox();

    void setSpecialValueText(const QString &text) { spin->setSpecialValueText(text); }
    void setSuffix(const QString &text) { spin->setSuffix(text); }
    void setSingleStep(int v) { spin->setSingleStep(v); }
    void setMinimum(int v) { spin->setMinimum(v); }
    void setMaximum(int v) { spin->setMaximum(v); }
    void setRange(int min, int max) { spin->setRange(min, max); }
    void setValue(int v);
    int value() const { return spin->value(); }
    int minimum() const{ return spin->minimum(); }
    int maximum() const { return spin->maximum(); }
    void setFocus() const { spin->setFocus(); }
    void setAllowEmpty() { spin->setAllowEmpty(); }

Q_SIGNALS:
    void valueChanged(int v);

private Q_SLOTS:
    void incPressed();
    void decPressed();
    void checkValue();

private:
    bool eventFilter(QObject *obj, QEvent *event);
    void paintEvent(QPaintEvent *e);

private:
    EmptySpinBox *spin;
    QToolButton *incButton;
    QToolButton *decButton;
};
#endif // Q_WS_WIN

#endif
