/****************************************************************************
**
** Copyright (c) 2007 Trolltech ASA <info@trolltech.com>
**
** Use, modification and distribution is allowed without limitation,
** warranty, liability or support of any kind.
**
****************************************************************************/

#include "lineedit.h"
#include <QToolButton>
#include <QStyle>
#include <QIcon>
#include <QDebug>

LineEdit::LineEdit(QWidget *parent)
    : QLineEdit(parent)
{
    clearButton = new QToolButton(this);
    QPixmap pixmap(QIcon::fromTheme(Qt::RightToLeft==layoutDirection() ? "edit-clear-locationbar-ltr" : "edit-clear-locationbar-rtl").pixmap(16, 16));
    clearButton->setIcon(QIcon(pixmap));
    clearButton->setIconSize(pixmap.size());
    clearButton->setCursor(Qt::ArrowCursor);
    clearButton->setStyleSheet("QToolButton { border: none; padding: 0px; }");
    clearButton->hide();
    connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));
    connect(this, SIGNAL(textChanged(const QString&)), this, SLOT(updateCloseButton(const QString&)));
    int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    setStyleSheet(QString(Qt::RightToLeft==layoutDirection() ? "QLineEdit { padding-left: %1px; } " : "QLineEdit { padding-right: %1px; } ").arg(clearButton->sizeHint().width() + frameWidth + 1));
    QSize msz = minimumSizeHint();
    setMinimumSize(qMax(msz.width(), clearButton->sizeHint().height() + frameWidth * 2 + 2),
                   qMax(msz.height(), clearButton->sizeHint().height() + frameWidth * 2 + 2));
}

void LineEdit::resizeEvent(QResizeEvent *)
{
    QSize sz = clearButton->sizeHint();
    int frameWidth = style()->pixelMetric(QStyle::PM_DefaultFrameWidth);
    if (Qt::RightToLeft==layoutDirection()) {
        clearButton->move(rect().left() + frameWidth,
                          (rect().bottom() + 1 - sz.height()) / 2);
    } else {
        clearButton->move(rect().right() - frameWidth - sz.width(),
                          (rect().bottom() + 1 - sz.height()) / 2);
    }
}

void LineEdit::updateCloseButton(const QString& text)
{
    clearButton->setVisible(!text.isEmpty());
}

