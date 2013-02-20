#include "osthumb.h"

#include <QTimer>
#include <QPainter>
#include <QApplication>
#include <QFontMetrics>
#include <QMouseEvent>

static int thumbWidth=20;
static int thumbHeight=72;
//#define THUMB_WIDTH          20
//#define THUMB_HEIGHT         72
#define THUMB_WIDTH thumbWidth
#define THUMB_HEIGHT thumbHeight
#define OUTLINE_THICKNESS     2
#define ANIMATION_TIMEOUT    20
#define HIDE_TIMEOUT        100
#define ANIMATION_ALPHA_INC  15

///*
// *  Variables used only for implementing methods.
// */
//QTimer *animationTimer; // The timer used to set the timeout for updating the alpha of the thumb
//                        // (used for fade in/out animation).
//QTimer *hideTimer;
//int alpha; // Current alpha of the thumb.
//int yPos;  // The position the thumb is to be moved (when aligned vertically).
//int xPos;  // Same as above (when aligned horizontally).
//bool mouseButtonPressed; // Holds the state of the left mouse button.
//bool wasDragged; // The widget has been dragged (neded to separate click on page up/down and drag action).

class OsThumbPrivate
{
    Q_DECLARE_PUBLIC(OsThumb)

public:
    OsThumbPrivate(OsThumb *q);
    bool hidden;
    Qt::Orientation orientation;
    int minimum;
    int maximum;
    OsThumb *q_ptr;
    void checkForLeave(QMouseEvent *event);

    /*
     *  Variables used only for implementing methods.
     */
    QTimer *animationTimer; // The timer used to set the timeout for updating the alpha of the thumb
                            // (used for fade in/out animation).
    QTimer *hideTimer;
    int alpha; // Current alpha of the thumb.
    int yPos;  // The position the thumb is to be moved (when aligned vertically).
    int xPos;  // Same as above (when aligned horizontally).
    bool mouseButtonPressed; // Holds the state of the left mouse button.
    bool wasDragged; // The widget has been dragged (neded to separate click on page up/down and drag action).
};

OsThumbPrivate::OsThumbPrivate(OsThumb *q) : q_ptr(q)
{
    /*
     *  Initialize variables used only in implementation.
     */
    alpha = 0;
    animationTimer = new QTimer(q);
    hideTimer = new QTimer(q);
    hideTimer->setSingleShot(true);
    wasDragged = false;
}

void OsThumbPrivate::checkForLeave(QMouseEvent *event)
{
    Q_Q(OsThumb);

    if (mouseButtonPressed) {
        return;
    }
    if (orientation == Qt::Vertical) {
        if ((event->globalX() >= q->geometry().x() + q->width()) ||
            (event->globalY() < q->geometry().y()) ||
            (event->globalY() >= q->geometry().y() + q->height())
           )
            hideTimer->start(HIDE_TIMEOUT);
    } else {
        if ((event->globalY() >= q->geometry().y() + q->height()) ||
            (event->globalX() < q->geometry().x()) ||
            (event->globalX() >= q->geometry().x() + q->width())
           )
            hideTimer->start(HIDE_TIMEOUT);
    }
}

OsThumb::OsThumb(Qt::Orientation o, QWidget *parent) :
    QWidget(parent), d_ptr(new OsThumbPrivate(this))
{
    int fh=QApplication::fontMetrics().height();
    thumbWidth=qMax(20, fh);
    thumbWidth=(((int)(thumbWidth/2))*2)+(thumbWidth%2 ? 2 : 0);
    thumbHeight=thumbWidth*3.5;

    /*
     *  Set up orientation.
     */
    setOrientation(o);
    if (o == Qt::Vertical)
        setFixedSize(THUMB_WIDTH, THUMB_HEIGHT);
    else
        setFixedSize(THUMB_HEIGHT, THUMB_WIDTH);

    /*
     *  Set some window properties. The thumb needs to be a separate window with no border, no background
     *  and it needs to bypass the window manager in order to have a custom behaviour.
     *  (Thank you Andrea for the idea)
     */
    setWindowFlags(Qt::X11BypassWindowManagerHint);
    setAttribute(Qt::WA_TranslucentBackground);

    /*
     *  Initialize class properties.
     */
    setHidden(false);
    setMouseTracking(true);

//    /*
//     *  Initialize variables used only in implementation.
//     */
//    alpha = 0;
//    animationTimer = new QTimer(this);
//    hideTimer = new QTimer(this);
//    hideTimer->setSingleShot(true);
//    wasDragged = false;

    /*
     *  Connect the animationTimer to the update() method so that the paintEvent() is triggered and the thumb is
     *  redrawn with a new alpha. The timer stops when alpha reaches 255 (fade in) or 0 (fade out). Also connect
     *  the hideTimer so that when the mouse pointer leaves the thumb it hides after 200 ms if there is no other
     *  action performed on the thumb.
     */
    QObject::connect(d_ptr->animationTimer, SIGNAL(timeout()), this, SLOT(update()));
    QObject::connect(d_ptr->hideTimer, SIGNAL(timeout()), this, SLOT(hide()));
}

void OsThumb::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    Q_D(OsThumb);
    /*
         *  Initialize the painter and set up the colors that are going to be used for drawing the thumb's
         *  elements.
         */
        QPainter painter(this);
        QPalette palette;
        QColor thumbStartColor = QColor::fromRgb(0xf2f1f0);
        QColor thumbStopColor = QColor::fromRgb(0xd2dedc);
        QColor outlineColor = palette.color(QPalette::Highlight);
        QColor arrowColor(Qt::black);
        QColor etchColor(Qt::white);
        QLinearGradient fill(1, 1, THUMB_WIDTH - 2, THUMB_HEIGHT - 2);

        int start=(THUMB_WIDTH/4)*1.5;
        int size=THUMB_WIDTH/5;
        QPoint pgUpPoints[] = {
            QPoint(THUMB_WIDTH/2, start),
            QPoint(THUMB_WIDTH/2 + size, start+size+1),
            QPoint(THUMB_WIDTH/2 - size, start+size+1)
        };
        QPoint pgDownPoints[] = {
            QPoint(THUMB_WIDTH/2, THUMB_HEIGHT - start/*75*/),
            QPoint(THUMB_WIDTH/2 + size, THUMB_HEIGHT - (start+size+1)/*69*/),
            QPoint(THUMB_WIDTH/2 - size, THUMB_HEIGHT - (start+size+1)/*69*/)
        };

        /*
         *  The folowing condition is responsible for the alpha setting during the animation. If the thumb
         *  is hidden that means that is about to be drawn therefor we increment the alpha value by
         *  ANIMATION_ALPHA_INC, once the alpha is at it's maximum value (255) we stop the timer. If the
         *  thumb is shown (!hidden) we decrement the alpha value by ANIMATION_ALPHA_INC until it reaches
         *  it's minimum (0), at which time we stop the timer and hide the widget.
         */
        if (hidden())
        {
            if (d->alpha >= 255)
            {
                d->animationTimer->stop();
            }
            else
                d->alpha += ANIMATION_ALPHA_INC;
            if (d->alpha>100) {
                emit showing();
            }
        }
        else
        {
            if (d->alpha <= 0)
            {
                d->animationTimer->stop();
                setVisible(false);
            }
            else
                d->alpha -= ANIMATION_ALPHA_INC;
            if (d->alpha<100) {
                emit hidding();
            }
        }

        /*
         *  Set the current alpha for each individual color of the widget.
         */
        thumbStartColor.setAlpha(d->alpha);
        thumbStopColor.setAlpha(d->alpha);
        outlineColor.setAlpha(d->alpha);
        arrowColor.setAlpha(d->alpha*0.5);
        etchColor.setAlpha(d->alpha*0.5);

        /*
         *  Set up the gradient used for filling the widget
         */
        fill.setColorAt(0, thumbStartColor);
        fill.setColorAt(0.5, thumbStopColor);
        fill.setColorAt(1, thumbStartColor);

        /*
         *  Save the current painter state onto the stack
         */
        painter.save();

        /*
         *  Set up antialiasing
         */
        painter.setRenderHint(QPainter::Antialiasing);

        /*
         *  Check if the widget is horizontal, and if it is apply required transformations to the painter and thumb
         */
        if (orientation() == Qt::Horizontal)
        {
            painter.translate(THUMB_HEIGHT, 0);
            painter.rotate(90);
        }

        /*
         *  Draw the widget's contents
         */
        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(fill));
        painter.drawRect(1, 1, THUMB_WIDTH - 2, THUMB_HEIGHT - 2);

        painter.setPen(QPen(outlineColor, OUTLINE_THICKNESS));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(1, 1, THUMB_WIDTH - 2, THUMB_HEIGHT - 2);

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(arrowColor, Qt::SolidPattern));
        painter.drawConvexPolygon(pgUpPoints, 3);
        painter.drawConvexPolygon(pgDownPoints, 3);

        arrowColor.setAlphaF(arrowColor.alphaF()*0.4);
        painter.setPen(arrowColor);
        painter.drawLine(QPointF(3.5, (THUMB_HEIGHT/2)+0.5), QPointF(THUMB_WIDTH-3.5, (THUMB_HEIGHT/2)+0.5));
        painter.setPen(etchColor);
        painter.drawLine(QPointF(3.5, (THUMB_HEIGHT/2)+1.5), QPointF(THUMB_WIDTH-3.5, (THUMB_HEIGHT/2)+1.5));

        // TODO: Draw dots...

        /*
         *  Pop the new painter state from the stack
         */
        painter.restore();
}

void OsThumb::show()
{
    Q_D(OsThumb);
    /*
     *  Show the thumb (in case it was not already shown), in case it was fading out stop the animation,
     *  set the thumb's current state (visibility wise) (hidden property) to true in order for
     *  the paintEvent to start incrementing the alpha value of the thumb, and start the animation again.
     */
    setVisible(true);
    d->animationTimer->stop();
    setHidden(true);
    d->animationTimer->start(ANIMATION_TIMEOUT);
}

void OsThumb::hide()
{
    Q_D(OsThumb);
    /*
     *  In case it was fading out stop the animation, set the thumb's current state (visibility wise)
     *  (hidden property) to false in order for the paintEvent to start decrementing the alpha value
     *  of the thumb, and start the animation again.
     */
    d->animationTimer->stop();
    setHidden(false);
    d->animationTimer->start(ANIMATION_TIMEOUT);
}

void OsThumb::mousePressEvent(QMouseEvent *event)
{
    Q_D(OsThumb);
    d->hideTimer->stop();
    /*
     *  Verify that the left mouse button was pressed and save the position at which the event
     *  occurred. Set the mouseButtonPressed property to true in order to initiate dragging in case
     *  the user moves the mouse (mouseMoveEvent occurres) while the left mouse button is pressed.
     */
    if (event->button() == Qt::LeftButton)
    {
        d->yPos = event->pos().y();
        d->xPos = event->pos().x();
        d->mouseButtonPressed = true;
    }
    //else QApplication::quit();
}

void OsThumb::mouseMoveEvent(QMouseEvent *event)
{
    Q_D(OsThumb);

    d->hideTimer->stop();
    d->checkForLeave(event);
    /*
     *  If the mouse button is pressed and the user starts moving the mouse; move the thumb
     *  to the mouse's Y position (vertical) or X position (horizontal). Also don't move the thumb
     *  outside the proximity region's boundries. Set wasDragged property to true so that when
     *  the mouseReleaseEvent occurs don't initiate a pageUp or pageDown movement within the
     *  scrollable content.
     */
    if (d->mouseButtonPressed) {
        int toXPos = x();
        int toYPos = y();

        if (orientation() == Qt::Vertical) {
            // vertical movement
            toYPos = event->globalY() - d->yPos;
            int minYPos = minimum();
            int maxYPos = maximum();

            if (toYPos < minYPos)
                toYPos = minYPos;
            if (toYPos > maxYPos)
                toYPos = maxYPos;
            // end vertical movement
        } else {
            // horizontal movement
            toXPos = event->globalX() - d->xPos;
            int minXPos = minimum();
            int maxXPos = maximum();

            if (toXPos < minXPos)
                toXPos = minXPos;
            if (toXPos > maxXPos)
                toXPos = maxXPos;
            // end horizontal movement
        }

        move(toXPos, toYPos);

        d->wasDragged = true;

        emit thumbDragged(QPoint(toXPos,toYPos));
    }
}

void OsThumb::mouseReleaseEvent(QMouseEvent *event)
{
    Q_D(OsThumb);

    d->hideTimer->stop();

    /*
     *  When the mouse button is released set mouseButton pressed to false, verify if the thumb has beed dragged
     *  if it has then check if the mouse pointer has left the thumb's surface and hide it or not accordingly.
     */
    d->mouseButtonPressed = false;

    if (d->wasDragged) {
        d->wasDragged = false;

        d->checkForLeave(event);

    /*
     *  If the thumb wasn't dragged, based on orientation, the user clicked the top/bottom, lef/right arrow and
     *  the content should scroll one page in the respective direction.
     */
    } else {
        if (orientation() == Qt::Vertical)
        {
            if (event->y() < THUMB_HEIGHT / 2)
                emit pageUp();
            if (event->y() > THUMB_HEIGHT / 2)
                emit pageDown();
        }
        else
        {
            if (event->x() < THUMB_HEIGHT / 2)
                emit pageUp();
            if (event->x() > THUMB_HEIGHT / 2)
                emit pageDown();
        }
    }
}

bool OsThumb::isVisible() const
{
    return hidden();
}

Qt::Orientation OsThumb::orientation() const
{
    Q_D(const OsThumb);
    return d->orientation;
}

void OsThumb::setOrientation(Qt::Orientation o)
{
    Q_D(OsThumb);
    d->orientation = o;
    if (o == Qt::Vertical)
        setFixedSize(THUMB_WIDTH, THUMB_HEIGHT);
    else
        setFixedSize(THUMB_HEIGHT, THUMB_WIDTH);
}

int OsThumb::minimum() const
{
    Q_D(const OsThumb);
    return d->minimum;
}

int OsThumb::maximum() const
{
    Q_D(const OsThumb);
    return d->maximum;
}

void OsThumb::setMinimum(int minimum)
{
    Q_D(OsThumb);
    d->minimum = minimum;
}

void OsThumb::setMaximum(int maximum)
{
    Q_D(OsThumb);
    d->maximum = maximum;
}

bool OsThumb::hidden() const
{
    Q_D(const OsThumb);
    return d->hidden;
}

void OsThumb::setHidden(bool hidden)
{
    Q_D(OsThumb);
    d->hidden = hidden;
}
