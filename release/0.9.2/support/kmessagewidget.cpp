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

#ifdef ENABLE_KDE_SUPPORT
#include <kaction.h>
#include <kcolorscheme.h>
#include <kdebug.h>
#include <kglobalsettings.h>
#include <kicon.h>
#include <kiconloader.h>
#include <kstandardaction.h>
#endif

#include "icon.h"
#include <QtCore/QEvent>
#include <QtGui/QGridLayout>
#include <QtGui/QHBoxLayout>
#include <QtGui/QLabel>
#include <QtGui/QPainter>
#include <QtGui/QShowEvent>
#include <QtCore/QTimeLine>
#include <QtGui/QToolButton>
#include <QtGui/QStyle>
#include <QtGui/QAction>

//---------------------------------------------------------------------
// KMsgWidgetPrivate
//---------------------------------------------------------------------
#if 0
class KMsgWidgetPrivate
{
public:
    void init(KMsgWidget*);

    KMsgWidget* q;
    QFrame* content;
    QLabel* iconLabel;
    QLabel* textLabel;
    QToolButton* closeButton;
    QTimeLine* timeLine;

    KMsgWidget::MessageType messageType;
    bool wordWrap;
    QList<QToolButton*> buttons;
    QPixmap contentSnapShot;

    void createLayout();
    void updateSnapShot();
    void updateLayout();
    void slotTimeLineChanged(qreal);
    void slotTimeLineFinished();
};
#endif

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

    iconLabel = new QLabel(content);
    iconLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    textLabel = new QLabel(content);
    textLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    textLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);

    #ifdef ENABLE_KDE_SUPPORT
    KAction* closeAction = KStandardAction::close(q, SLOT(animatedHide()), q);
    #else
    QAction* closeAction = new QAction(q);
    closeAction->setIcon(Icon("dialog-close"));
    QObject::connect(closeAction, SIGNAL(triggered()), q, SLOT(animatedHide()));
    #endif

    closeButton = new QToolButton(content);
    closeButton->setAutoRaise(true);
    closeButton->setDefaultAction(closeAction);

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
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        buttons.append(button);
    }

    // AutoRaise reduces visual clutter, but we don't want to turn it on if
    // there are other buttons, otherwise the close button will look different
    // from the others.
    closeButton->setAutoRaise(buttons.isEmpty());

    if (wordWrap) {
        QGridLayout* layout = new QGridLayout(content);
        // Set alignment to make sure icon does not move down if text wraps
        layout->addWidget(iconLabel, 0, 0, 1, 1, Qt::AlignHCenter | Qt::AlignTop);
        layout->addWidget(textLabel, 0, 1);

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
        layout->addWidget(iconLabel);
        layout->addWidget(textLabel);

        Q_FOREACH(QToolButton* button, buttons) {
            layout->addWidget(button);
        }

        layout->addWidget(closeButton);
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
        content->move(0, 0);
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

#ifdef ENABLE_KDE_SUPPORT
static void getColorsFromColorScheme(KColorScheme::BackgroundRole bgRole, QColor* bg, QColor* fg)
{
    KColorScheme scheme(QPalette::Active, KColorScheme::Window);
    *bg = scheme.background(bgRole).color();
    *fg = scheme.foreground().color();
}
#endif

void KMsgWidget::setMessageType(KMsgWidget::MessageType type)
{
    d->messageType = type;
    QIcon icon;
    QColor bg0, bg1, bg2, border, fg;
    switch (type) {
    case Positive:
        icon = QIcon::fromTheme("dialog-ok");
        #ifdef ENABLE_KDE_SUPPORT
        getColorsFromColorScheme(KColorScheme::PositiveBackground, &bg1, &fg);
        #else
        bg1=QColor(0xAB, 0xC7, 0xED);
        fg=Qt::black;
        border = Qt::blue;
        #endif
        break;
    case Information:
        icon = QIcon::fromTheme("dialog-information");
        // There is no "information" background role in KColorScheme, use the
        // colors of highlighted items instead
        bg1 = palette().highlight().color();
        fg = palette().highlightedText().color();
        #ifdef ENABLE_KDE_SUPPORT
        border = bg1.darker(150);
        #endif
        break;
    case Warning:
        icon = QIcon::fromTheme("dialog-warning");
        #ifdef ENABLE_KDE_SUPPORT
        getColorsFromColorScheme(KColorScheme::NeutralBackground, &bg1, &fg);
        #else
        bg1=QColor(0xED, 0xC6, 0x62);
        fg=Qt::black;
        border = Qt::red;
        #endif
        break;
    case Error:
        icon = QIcon::fromTheme("dialog-error");
        #ifdef ENABLE_KDE_SUPPORT
        getColorsFromColorScheme(KColorScheme::NegativeBackground, &bg1, &fg);
        #else
        bg1=QColor(0xeb, 0xbb, 0xbb);
        fg=Qt::black;
        border = Qt::red;
        #endif
        break;
    }

    // Colors
    bg0 = bg1.lighter(110);
    bg2 = bg1.darker(110);
    #ifdef ENABLE_KDE_SUPPORT
    border = KColorScheme::shade(bg1, KColorScheme::DarkShade);
    #endif

    d->content->setStyleSheet(
        QString(".QFrame {"
            "background-color: qlineargradient(x1: 0, y1: 0, x2: 0, y2: 1,"
            "    stop: 0 %1,"
            "    stop: 0.1 %2,"
            "    stop: 1.0 %3);"
            "border-radius: 5px;"
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
        .arg(style()->pixelMetric(QStyle::PM_DefaultFrameWidth, 0, this) -1)
        .arg(fg.name())
        );

    // Icon
    #ifdef ENABLE_KDE_SUPPORT
    const int size = KIconLoader::global()->currentSize(KIconLoader::MainToolbar);
    #else
    const int size = 22;
    #endif
    d->iconLabel->setPixmap(icon.pixmap(size));
}

QSize KMsgWidget::sizeHint() const
{
    ensurePolished();
    return d->content->sizeHint();
}

QSize KMsgWidget::minimumSizeHint() const
{
    ensurePolished();
    return d->content->minimumSizeHint();
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
    if (!isVisible()) return;
    int contentHeight = d->bestContentHeight();

    if (d->timeLine->state() == QTimeLine::NotRunning) {
        d->content->resize(width(), contentHeight);
    } else if (event->size().width() != event->oldSize().width()) {
        d->content->resize(width(), contentHeight);
        d->updateSnapShot();
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

void KMsgWidget::showEvent(QShowEvent* event)
{
    // Keep this method here to avoid breaking binary compatibility:
    // QFrame::showEvent() used to be reimplemented.
    QFrame::showEvent(event);
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
    #ifdef ENABLE_KDE_SUPPORT
    if (!(KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects)) {
        show();
        return;
    }
    #endif

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
    #ifdef ENABLE_KDE_SUPPORT
    if (!(KGlobalSettings::graphicEffectsLevel() & KGlobalSettings::SimpleAnimationEffects)) {
        hide();
        return;
    }
    #endif

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

