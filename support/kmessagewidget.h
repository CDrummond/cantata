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
#ifndef KMESSAGEWIDGET_H
#define KMESSAGEWIDGET_H

#include <QtGui/QFrame>

class KMessageWidgetPrivate;

/**
 * @short A widget to provide feedback or propose opportunistic interactions.
 *
 * KMessageWidget can be used to provide inline positive or negative
 * feedback, or to implement opportunistic interactions.
 *
 * As a feedback widget, KMessageWidget provides a less intrusive alternative
 * to "OK Only" message boxes. If you do not need the modalness of KMessageBox,
 * consider using KMessageWidget instead.
 *
 * <b>Negative feedback</b>
 *
 * The KMessageWidget can be used as a secondary indicator of failure: the
 * first indicator is usually the fact the action the user expected to happen
 * did not happen.
 *
 * Example: User fills a form, clicks "Submit".
 *
 * @li Expected feedback: form closes
 * @li First indicator of failure: form stays there
 * @li Second indicator of failure: a KMessageWidget appears on top of the
 * form, explaining the error condition
 *
 * When used to provide negative feedback, KMessageWidget should be placed
 * close to its context. In the case of a form, it should appear on top of the
 * form entries.
 *
 * KMessageWidget should get inserted in the existing layout. Space should not
 * be reserved for it, otherwise it becomes "dead space", ignored by the user.
 * KMessageWidget should also not appear as an overlay to prevent blocking
 * access to elements the user needs to interact with to fix the failure.
 *
 * <b>Positive feedback</b>
 *
 * KMessageWidget can be used for positive feedback but it shouldn't be
 * overused. It is often enough to provide feedback by simply showing the
 * results of an action.
 *
 * Examples of acceptable uses:
 *
 * @li Confirm success of "critical" transactions
 * @li Indicate completion of background tasks
 *
 * Example of inadapted uses:
 *
 * @li Indicate successful saving of a file
 * @li Indicate a file has been successfully removed
 *
 * <b>Opportunistic interaction</b>
 *
 * Opportunistic interaction is the situation where the application suggests to
 * the user an action he could be interested in perform, either based on an
 * action the user just triggered or an event which the application noticed.
 *
 * Example of acceptable uses:
 *
 * @li A browser can propose remembering a recently entered password
 * @li A music collection can propose ripping a CD which just got inserted
 * @li A chat application may notify the user a "special friend" just connected
 *
 * @author Aurélien Gâteau <agateau@kde.org>
 * @since 4.7
 */
class KMessageWidget : public QFrame
{
    Q_OBJECT
    Q_ENUMS(MessageType)

    Q_PROPERTY(QString text READ text WRITE setText)
    Q_PROPERTY(bool wordWrap READ wordWrap WRITE setWordWrap)
    Q_PROPERTY(bool closeButtonVisible READ isCloseButtonVisible WRITE setCloseButtonVisible)
    Q_PROPERTY(MessageType messageType READ messageType WRITE setMessageType)
public:
    enum MessageType {
        Positive,
        Information,
        Warning,
        Error
    };

    /**
     * Constructs a KMessageWidget with the specified parent.
     */
    explicit KMessageWidget(QWidget *parent = 0);

    explicit KMessageWidget(const QString &text, QWidget *parent = 0);

    ~KMessageWidget();

    QString text() const;

    bool wordWrap() const;

    bool isCloseButtonVisible() const;

    MessageType messageType() const;

    void addAction(QAction *action);

    void removeAction(QAction *action);

    QSize sizeHint() const;

    QSize minimumSizeHint() const;

    int heightForWidth(int width) const;

public Q_SLOTS:
    void setText(const QString &text);

    void setWordWrap(bool wordWrap);

    void setCloseButtonVisible(bool visible);

    void setMessageType(KMessageWidget::MessageType type);

    /**
     * Show the widget using an animation, unless
     * KGlobalSettings::graphicsEffectLevel() does not allow simple effects.
     */
    void animatedShow();

    /**
     * Hide the widget using an animation, unless
     * KGlobalSettings::graphicsEffectLevel() does not allow simple effects.
     */
    void animatedHide();

protected:
    void paintEvent(QPaintEvent *event);

    bool event(QEvent *event);

    void resizeEvent(QResizeEvent *event);

    void showEvent(QShowEvent *event);

private:
    KMessageWidgetPrivate *const d;
    friend class KMessageWidgetPrivate;

    Q_PRIVATE_SLOT(d, void slotTimeLineChanged(qreal))
    Q_PRIVATE_SLOT(d, void slotTimeLineFinished())
};

class QTimeLine;
class QLabel;
class QToolButton;
class KMessageWidgetPrivate
{
public:
    void init(KMessageWidget*);

    KMessageWidget* q;
    QFrame* content;
    QLabel* iconLabel;
    QLabel* textLabel;
    QToolButton* closeButton;
    QTimeLine* timeLine;

    KMessageWidget::MessageType messageType;
    bool wordWrap;
    QList<QToolButton*> buttons;
    QPixmap contentSnapShot;

    void createLayout();
    void updateSnapShot();
    void updateLayout();
    void slotTimeLineChanged(qreal);
    void slotTimeLineFinished();
};

#endif /* KMESSAGEWIDGET_H */
