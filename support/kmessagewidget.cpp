/* This file is part of the KDE libraries
 *
 * Copyright (c) 2011 Aurélien Gâteau <agateau@kde.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */
#include "kmessagewidget.h"
#include "utils.h"
#include "monoicon.h"
#include "icon.h"
#include "squeezedtextlabel.h"
#include "flattoolbutton.h"
#include <QEvent>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QShowEvent>
#include <QTimeLine>
#include <QToolButton>
#include <QStyle>
#include <QAction>

void KMsgWidgetPrivate::init(KMsgWidget *q_ptr)
{
    q = q_ptr;

    q->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    timeLine = new QTimeLine(500, q);
    QObject::connect(timeLine, SIGNAL(valueChanged(qreal)), q, SLOT(slotTimeLineChanged(qreal)));
    QObject::connect(timeLine, SIGNAL(finished()), q, SLOT(slotTimeLineFinished()));

    content = new QFrame(q);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    wordWrap = false;

    textLabel = new SqueezedTextLabel(content);
    textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    textLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    textLabel->setOpenExternalLinks(true);

    QAction* closeAction = new QAction(q);
    closeAction->setIcon(MonoIcon::icon(FontAwesome::close, MonoIcon::constRed, MonoIcon::constRed));
    closeAction->setToolTip(QObject::tr("Close"));
    QObject::connect(closeAction, SIGNAL(triggered()), q, SLOT(animatedHide()));

    closeButton = new FlatToolButton(content);
    closeButton->setDefaultAction(closeAction);
    #ifdef Q_OS_MAC
    closeButton->setStyleSheet("QToolButton {border: 0}");
    #endif
    q->setMessageType(KMsgWidget::Information);
}

void KMsgWidgetPrivate::createLayout()
{
    delete content->layout();

    content->resize(q->size());

    qDeleteAll(buttons);
    buttons.clear();

    Q_FOREACH(QAction* action, q->actions()) {
        QToolButton* button = new QToolButton(content);
        button->setDefaultAction(action);
        if (content->style()->styleHint(QStyle::SH_DialogButtonBox_ButtonsHaveIcons)) {
            button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        } else {
            button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        }
        buttons.append(button);
    }

    // AutoRaise reduces visual clutter, but we don't want to turn it on if
    // there are other buttons, otherwise the close button will look different
    // from the others.
    closeButton->setAutoRaise(buttons.isEmpty());

    if (wordWrap) {
        QGridLayout* layout = new QGridLayout(content);
        layout->addWidget(textLabel, 0, 1);
        layout->setMargin(4);

        QHBoxLayout* buttonLayout = new QHBoxLayout;
        buttonLayout->addStretch();
        Q_FOREACH(QToolButton* button, buttons) {
            // For some reason, calling show() is necessary if wordwrap is true,
            // otherwise the buttons do not show up. It is not needed if
            // wordwrap is false.
            button->show();
            buttonLayout->addWidget(button);
        }
        buttonLayout->addWidget(closeButton);
        layout->addItem(buttonLayout, 1, 0, 1, 2);
    } else {
        QHBoxLayout* layout = new QHBoxLayout(content);
        layout->addWidget(textLabel);

        Q_FOREACH(QToolButton* button, buttons) {
            layout->addWidget(button);
        }

        layout->addWidget(closeButton);
        layout->setMargin(4);
    };

    if (q->isVisible()) {
        q->setFixedHeight(content->sizeHint().height());
    }
    q->updateGeometry();
}

void KMsgWidgetPrivate::updateLayout()
{
    if (content->layout()) {
        createLayout();
    }
}

void KMsgWidgetPrivate::updateSnapShot()
{
    // Attention: updateSnapShot calls QWidget::render(), which causes the whole
    // window layouts to be activated. Calling this method from resizeEvent()
    // can lead to infinite recursion, see:
    // https://bugs.kde.org/show_bug.cgi?id=311336
    contentSnapShot = QPixmap(content->size());
    contentSnapShot.fill(Qt::transparent);
    content->render(&contentSnapShot, QPoint(), QRegion(), QWidget::DrawChildren);
}

void KMsgWidgetPrivate::slotTimeLineChanged(qreal value)
{
    q->setFixedHeight(qMin(value * 2, qreal(1.0)) * content->height());
    q->update();
}

void KMsgWidgetPrivate::slotTimeLineFinished()
{
    if (timeLine->direction() == QTimeLine::Forward) {
        // Show
        // We set the whole geometry here, because it may be wrong if a
        // KMessageWidget is shown right when the toplevel window is created.
        content->setGeometry(0, 0, q->width(), bestContentHeight());
    } else {
        // Hide
        q->hide();
    }
}

int KMsgWidgetPrivate::bestContentHeight() const
{
    int height = content->heightForWidth(q->width());
    if (height == -1) {
        height = content->sizeHint().height();
    }
    return height;
}

//---------------------------------------------------------------------
// KMsgWidget
//---------------------------------------------------------------------
KMsgWidget::KMsgWidget(QWidget* parent)
    : QFrame(parent)
    , d(new KMsgWidgetPrivate)
{
    d->init(this);
}

KMsgWidget::KMsgWidget(const QString& text, QWidget* parent)
    : QFrame(parent)
    , d(new KMsgWidgetPrivate)
{
    d->init(this);
    setText(text);
}

KMsgWidget::~KMsgWidget()
{
    delete d;
}

QString KMsgWidget::text() const
{
    return d->textLabel->text();
}

void KMsgWidget::setText(const QString& text)
{
    d->textLabel->setText(text);
    updateGeometry();
}

KMsgWidget::MessageType KMsgWidget::messageType() const
{
    return d->messageType;
}

void KMsgWidget::setMessageType(KMsgWidget::MessageType type)
{
    d->messageType = type;
    QColor bg0, bg1, bg2, border, fg;
    switch (type) {
    case Positive:
        bg1=QColor(0xAB, 0xC7, 0xED);
        fg=Qt::black;
        border = Qt::blue;
        break;
    case Information:
        // There is no "information" background role in KColorScheme, use the
        // colors of highlighted items instead
        bg1=QColor(0x98, 0xBC, 0xE3);
        fg=Qt::black;
        border = Qt::blue;
        break;
    case Warning:
        bg1=QColor(0xED, 0xC6, 0x62);
        fg=Qt::black;
        border = Qt::red;
        break;
    case Error:
        bg1=QColor(0xeb, 0xbb, 0xbb);
        fg=Qt::black;
        border = Qt::red;
        break;
    }

    // Colors
    bg0 = bg1.lighter(110);
    bg2 = bg1.darker(110);

    d->content->setStyleSheet(
        QString(".QFrame {"
            "background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
            "    stop: 0 %1,"
            "    stop: 0.1 %2,"
            "    stop: 1.0 %3);"
            "border: 1px solid %4;"
            "margin: %5px;"
            "}"
            ".QLabel { color: %6; }"
            )
        .arg(bg0.name())
        .arg(bg1.name())
        .arg(bg2.name())
        .arg(border.name())
        // DefaultFrameWidth returns the size of the external margin + border width. We know our border is 1px, so we subtract this from the frame normal QStyle FrameWidth to get our margin
        .arg(style()->pixelMetric(QStyle::PM_DefaultFrameWidth, nullptr, this) -1)
        .arg(fg.name())
        );
}

bool KMsgWidget::event(QEvent* event)
{
    if (event->type() == QEvent::Polish && !d->content->layout()) {
        d->createLayout();
    }
    return QFrame::event(event);
}

void KMsgWidget::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);

    if (d->timeLine->state() == QTimeLine::NotRunning) {
        d->content->resize(width(), d->bestContentHeight());
    }
}

int KMsgWidget::heightForWidth(int width) const
{
    ensurePolished();
    return d->content->heightForWidth(width);
}

void KMsgWidget::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    if (d->timeLine->state() == QTimeLine::Running) {
        QPainter painter(this);
        painter.setOpacity(d->timeLine->currentValue() * d->timeLine->currentValue());
        painter.drawPixmap(0, 0, d->contentSnapShot);
    }
}

bool KMsgWidget::wordWrap() const
{
    return d->wordWrap;
}

void KMsgWidget::setWordWrap(bool wordWrap)
{
    d->wordWrap = wordWrap;
    d->textLabel->setWordWrap(wordWrap);
    d->updateLayout();
}

bool KMsgWidget::isCloseButtonVisible() const
{
    return d->closeButton->isVisible();
}

void KMsgWidget::setCloseButtonVisible(bool show)
{
    d->closeButton->setVisible(show);
}

void KMsgWidget::addAction(QAction* action)
{
    QFrame::addAction(action);
    d->updateLayout();
}

void KMsgWidget::removeAction(QAction* action)
{
    QFrame::removeAction(action);
    d->updateLayout();
}

void KMsgWidget::animatedShow()
{
    if (isVisible()) {
        return;
    }

    QFrame::show();
    setFixedHeight(0);
    int wantedHeight = d->bestContentHeight();
    d->content->setGeometry(0, -wantedHeight, width(), wantedHeight);

    d->updateSnapShot();

    d->timeLine->setDirection(QTimeLine::Forward);
    if (d->timeLine->state() == QTimeLine::NotRunning) {
        d->timeLine->start();
    }
}

void KMsgWidget::animatedHide()
{
    if (!isVisible()) {
        return;
    }

    d->content->move(0, -d->content->height());
    d->updateSnapShot();

    d->timeLine->setDirection(QTimeLine::Backward);
    if (d->timeLine->state() == QTimeLine::NotRunning) {
        d->timeLine->start();
    }
}


#include "moc_kmessagewidget.cpp"
