/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */

// Copied from oxygenwindowmanager.cpp svnversion: 1139230

//////////////////////////////////////////////////////////////////////////////
// oxygenwindowmanager.cpp
// pass some window mouse press/release/move event actions to window manager
// -------------------
//
// Copyright (c) 2010 Hugo Pereira Da Costa <hugo@oxygen-icons.org>
//
// Largely inspired from BeSpin style
// Copyright (C) 2007 Thomas Luebking <thomas.luebking@web.de>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
//////////////////////////////////////////////////////////////////////////////

#include "windowmanager.h"
#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QDockWidget>
#include <QGroupBox>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QMenuBar>
#include <QMouseEvent>
#include <QStatusBar>
#include <QStyle>
#include <QStyleOptionGroupBox>
#include <QTabBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QProgressBar>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobalSettings>
#endif

#ifdef Q_WS_X11
#include <QX11Info>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/NETRootInfo>
#else
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include "fixx11h.h"
#endif
#endif

static inline bool isToolBar(QWidget *w)
{
    return qobject_cast<QToolBar*>(w) || 0==strcmp(w->metaObject()->className(), "ToolBar");
}

static inline void addEventFilter(QObject *object, QObject *filter)
{
    object->removeEventFilter(filter);
    object->installEventFilter(filter);
}

#if QT_VERSION < 0x040600
Pointer & Pointer::operator=(QWidget *w)
{
    widget_=w;
    if (widget_) {
        addEventFilter(widget_, this);
    }
    return *this;
}

void Pointer::clear()
{
    if (widget_) {
        widget_->removeEventFilter(this);
    }
    widget_=0L;
}

bool Pointer::eventFilter(QObject *o, QEvent *e)
{
    if (o==widget_ && QEvent::Destroy==e->type()) {
        widget_=0L;
    }
    return false;
}
#endif

WindowManager::WindowManager(QObject *parent):
    QObject(parent),
    #ifdef Q_WS_X11
    _useWMMoveResize(true),
    #else
    _useWMMoveResize(false),
    #endif
    _dragMode(WM_DRAG_NONE),
    #ifdef ENABLE_KDE_SUPPORT
    _dragDistance(KGlobalSettings::dndEventDelay()),
    #else
    _dragDistance(QApplication::startDragDistance()),
    #endif
    _dragDelay(QApplication::startDragTime()),
    _dragAboutToStart(false),
    _dragInProgress(false),
    _locked(false),
    _cursorOverride(false)
{
    // install application wise event filter
    _appEventFilter = new AppEventFilter(this);
    qApp->installEventFilter(_appEventFilter);
}

void WindowManager::initialize(int windowDrag)
{
    setDragMode(windowDrag);
    #ifdef ENABLE_KDE_SUPPORT
    setDragDistance(KGlobalSettings::dndEventDelay());
    #endif
    setDragDelay(QApplication::startDragTime());
}

void WindowManager::registerWidgetAndChildren(QWidget *w)
{
    QObjectList children=w->children();

    foreach (QObject *o, children) {
        if (qobject_cast<QWidget *>(o)) {
            registerWidgetAndChildren((QWidget *)o);
        }
    }
    registerWidget(w);
}

void WindowManager::registerWidget(QWidget *widget)
{
    if (isBlackListed(widget)) {
        addEventFilter(widget, this);
    } else if (isDragable(widget)) {
        addEventFilter(widget, this);
    }
}

void WindowManager::unregisterWidget(QWidget *widget)
{
    if (widget) {
        widget->removeEventFilter(this);
    }
}

bool WindowManager::eventFilter(QObject *object, QEvent *event)
{
    if (!enabled()) {
        return false;
    }

    switch (event->type())
    {
    case QEvent::MouseButtonPress:
        return mousePressEvent(object, event);
        break;
    case QEvent::MouseMove:
        if (object == _target.data()) {
            return mouseMoveEvent(object, event);
        }
        break;
    case QEvent::MouseButtonRelease:
        if (_target) {
            return mouseReleaseEvent(object, event);
        }
        break;
    default:
        break;
    }
    return false;
}

void WindowManager::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == _dragTimer.timerId()) {
        _dragTimer.stop();
        if (_target) {
            startDrag(_target.data(), _globalDragPoint);
        }
    } else {
        return QObject::timerEvent(event);
    }
}

bool WindowManager::mousePressEvent(QObject *object, QEvent *event)
{
    // cast event and check buttons/modifiers
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (!(Qt::NoModifier==mouseEvent->modifiers() && Qt::LeftButton==mouseEvent->button())) {
        return false;
    }

    // check lock
    if (isLocked()) {
        return false;
    } else {
        setLocked(true);
    }

    // cast to widget
    QWidget *widget = static_cast<QWidget*>(object);

    // check if widget can be dragged from current position
    if (isBlackListed(widget) || !canDrag(widget)) {
        return false;
    }

    // retrieve widget's child at event position
    QPoint position(mouseEvent->pos());
    QWidget *child = widget->childAt(position);
    if (!canDrag(widget, child, position)) {
        return false;
    }

    // save target and drag point
    _target = widget;
    _dragPoint = position;
    _globalDragPoint = mouseEvent->globalPos();
    _dragAboutToStart = true;

    // send a move event to the current child with same position
    // if received, it is caught to actually start the drag
    QPoint localPoint(_dragPoint);
    if (child) {
        localPoint = child->mapFrom(widget, localPoint);
    } else {
        child = widget;
    }
    QMouseEvent localMouseEvent(QEvent::MouseMove, localPoint, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    qApp->sendEvent(child, &localMouseEvent);
    // never eat event
    return false;
}

bool WindowManager::mouseMoveEvent(QObject *object, QEvent *event)
{
    Q_UNUSED(object);

    // stop timer
    if (_dragTimer.isActive()){
        _dragTimer.stop();
    }

    // cast event and check drag distance
    QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
    if (!_dragInProgress) {
        if (_dragAboutToStart) {
            if (mouseEvent->globalPos() == _globalDragPoint) {
                // start timer,
                _dragAboutToStart = false;
                if (_dragTimer.isActive()) {
                    _dragTimer.stop();
                }
                _dragTimer.start(_dragDelay, this);

            } else {
                resetDrag();
            }
        } else if (QPoint(mouseEvent->globalPos() - _globalDragPoint).manhattanLength() >= _dragDistance) {
            _dragTimer.start(0, this);
        }
        return true;
    } else if (!useWMMoveResize()) {
        // use QWidget::move for the grabbing
        /* this works only if the sending object and the target are identical */
        QWidget *window(_target.data()->window());
        window->move(window->pos() + mouseEvent->pos() - _dragPoint);
        return true;
    } else {
        return false;
    }
}

bool WindowManager::mouseReleaseEvent(QObject *object, QEvent *event)
{
    Q_UNUSED(object);
    Q_UNUSED(event);
    resetDrag();
    return false;
}

bool WindowManager::isDragable(QWidget *widget)
{
    // check widget
    if (!widget) {
        return false;
    }

    // accepted default types
    if ((qobject_cast<QDialog*>(widget) && widget->isWindow()) || (qobject_cast<QMainWindow*>(widget) && widget->isWindow()) || qobject_cast<QGroupBox*>(widget)) {
        return true;
    }

    // more accepted types, provided they are not dock widget titles
    if ((qobject_cast<QMenuBar*>(widget) || qobject_cast<QTabBar*>(widget) || qobject_cast<QStatusBar*>(widget) || isToolBar(widget)) &&
         !isDockWidgetTitle(widget)) {
        return true;
    }

    // flat toolbuttons
    if (QToolButton *toolButton = qobject_cast<QToolButton*>(widget)) {
        if (toolButton->autoRaise()) {
            return true;
        }
    }

    // viewports
    /*
        one needs to check that
        1/ the widget parent is a scrollarea
        2/ it matches its parent viewport
        3/ the parent is not blacklisted
        */
    if (QListView *listView = qobject_cast<QListView*>(widget->parentWidget())) {
        if (listView->viewport() == widget && !isBlackListed(listView)) {
            return true;
        }
    }

    if (QTreeView *treeView = qobject_cast<QTreeView*>(widget->parentWidget())) {
        if (treeView->viewport() == widget && !isBlackListed(treeView)) {
            return true;
        }
    }

    /*
        catch labels in status bars.
        this is because of kstatusbar
        who captures buttonPress/release events
        */
    if (QLabel *label = qobject_cast<QLabel*>(widget)) {
        if (label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse)) {
            return false;
        }

        QWidget *parent = label->parentWidget();
        while (parent) {
            if (qobject_cast<QStatusBar*>(parent)) {
                return true;
            }
            parent = parent->parentWidget();
        }
    }

    return false;
}

bool WindowManager::isBlackListed(QWidget *widget)
{
    QVariant propertyValue(widget->property("_kde_no_window_grab"));
    return propertyValue.isValid() && propertyValue.toBool();
}

bool WindowManager::canDrag(QWidget *widget)
{
    // check if enabled
    if (!enabled()) {
        return false;
    }

    // assume isDragable widget is already passed
    // check some special cases where drag should not be effective

    // check mouse grabber
    if (QWidget::mouseGrabber()) {
        return false;
    }

    /*
        check cursor shape.
        Assume that a changed cursor means that some action is in progress
        and should prevent the drag
        */
    if (Qt::ArrowCursor!=widget->cursor().shape()) {
        return false;
    }

    // accept
    return true;
}

bool WindowManager::canDrag(QWidget *widget, QWidget *child, const QPoint &position)
{
    // retrieve child at given position and check cursor again
    if (child && Qt::ArrowCursor!=child->cursor().shape()) {
        return false;
    }

    /*
        check against children from which drag should never be enabled,
        even if mousePress/Move has been passed to the parent
        */
    if (child && (qobject_cast<QComboBox*>(child) || qobject_cast<QProgressBar*>(child))) {
        return false;
    }

    // tool buttons
    if (QToolButton *toolButton = qobject_cast<QToolButton*>(widget)) {
        if (dragMode() < WM_DRAG_ALL && !isToolBar(widget->parentWidget())) {
            return false;
        }
        return toolButton->autoRaise() && !toolButton->isEnabled();
    }

    // check menubar
    if (QMenuBar *menuBar = qobject_cast<QMenuBar*>(widget)) {
        // check if there is an active action
        if (menuBar->activeAction() && menuBar->activeAction()->isEnabled()) {
            return false;
        }

        // check if action at position exists and is enabled
        if (QAction *action = menuBar->actionAt(position)) {
            if (action->isSeparator()) {
                return true;
            }
            if (action->isEnabled()) {
                return false;
            }
        }

        // return true in all other cases
        return true;
    }

    bool toolbar=isToolBar(widget);
    if (dragMode() < WM_DRAG_MENU_AND_TOOLBAR && toolbar) {
        return false;
    }

    /*
        in MINIMAL mode, anything that has not been already accepted
        and does not come from a toolbar is rejected
        */
    if (dragMode() < WM_DRAG_ALL) {
        return toolbar;
    }

    /* following checks are relevant only for WD_FULL mode */

    // tabbar. Make sure no tab is under the cursor
    if (QTabBar *tabBar = qobject_cast<QTabBar*>(widget)) {
        return -1==tabBar->tabAt(position);
    }

    /*
        check groupboxes
        prevent drag if unchecking grouboxes
        */
    if (QGroupBox *groupBox = qobject_cast<QGroupBox*>(widget)) {
        // non checkable group boxes are always ok
        if (!groupBox->isCheckable()) {
            return true;
        }
        // gather options to retrieve checkbox subcontrol rect
        QStyleOptionGroupBox opt;
        opt.initFrom(groupBox);
        if (groupBox->isFlat()) {
            opt.features |= QStyleOptionFrameV2::Flat;
        }
        opt.lineWidth = 1;
        opt.midLineWidth = 0;
        opt.text = groupBox->title();
        opt.textAlignment = groupBox->alignment();
        opt.subControls = (QStyle::SC_GroupBoxFrame | QStyle::SC_GroupBoxCheckBox);
        if (!groupBox->title().isEmpty()) {
            opt.subControls |= QStyle::SC_GroupBoxLabel;
        }

        opt.state |= (groupBox->isChecked() ? QStyle::State_On : QStyle::State_Off);

        // check against groupbox checkbox
        if (groupBox->style()->subControlRect(QStyle::CC_GroupBox, &opt, QStyle::SC_GroupBoxCheckBox, groupBox).contains(position)) {
            return false;
        }

        // check against groupbox label
        if (!groupBox->title().isEmpty() && groupBox->style()->subControlRect(QStyle::CC_GroupBox, &opt, QStyle::SC_GroupBoxLabel, groupBox).contains(position)) {
            return false;
        }

        return true;
    }

    // labels
    if (QLabel *label = qobject_cast<QLabel*>(widget)) { if (label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse)) {
            return false;
        }
    }

    // abstract item views
    QAbstractItemView *itemView(NULL);
    if ((itemView = qobject_cast<QListView*>(widget->parentWidget())) || (itemView = qobject_cast<QTreeView*>(widget->parentWidget()))) {
        if (widget == itemView->viewport()) {
            // QListView
            if (QFrame::NoFrame!=itemView->frameShape()) {
                return false;
            } else if (QAbstractItemView::NoSelection!=itemView->selectionMode() &&
                       QAbstractItemView::SingleSelection!=itemView->selectionMode() &&
                       itemView->model() && itemView->model()->rowCount()) {
                return false;
            } else if (itemView->model() && itemView->indexAt(position).isValid()) {
                return false;
            }
        }
    } else if ((itemView = qobject_cast<QAbstractItemView*>(widget->parentWidget()))) {
        if (widget == itemView->viewport()) {
            // QAbstractItemView
            if (QFrame::NoFrame!=itemView->frameShape()) {
                return false;
            } else if (itemView->indexAt(position).isValid()) {
                return false;
            }
        }
    }
    return true;
}

void WindowManager::resetDrag(void)
{
    if ((!useWMMoveResize()) && _target && _cursorOverride) {
        qApp->restoreOverrideCursor();
        _cursorOverride = false;
    }

    _target.clear();
    if (_dragTimer.isActive()) {
        _dragTimer.stop();

    }
    _dragPoint = QPoint();
    _globalDragPoint = QPoint();
    _dragAboutToStart = false;
    _dragInProgress = false;
}

void WindowManager::startDrag(QWidget *widget, const QPoint& position)
{
    if (!(enabled() && widget)) {
        return;
    }
    if (QWidget::mouseGrabber()) {
        return;
    }

    // ungrab pointer
    if (useWMMoveResize()) {
        #ifdef Q_WS_X11
        #ifndef ENABLE_KDE_SUPPORT
        static const Atom constNetMoveResize = XInternAtom(QX11Info::display(), "_NET_WM_MOVERESIZE", False);
        //...Taken from bespin...
        // stolen... errr "adapted!" from QSizeGrip
        QX11Info info;
        XEvent xev;
        xev.xclient.type = ClientMessage;
        xev.xclient.message_type = constNetMoveResize;
        xev.xclient.display = QX11Info::display();
        xev.xclient.window = widget->window()->winId();
        xev.xclient.format = 32;
        xev.xclient.data.l[0] = position.x();
        xev.xclient.data.l[1] = position.y();
        xev.xclient.data.l[2] = 8; // NET::Move
        xev.xclient.data.l[3] = Button1;
        xev.xclient.data.l[4] = 0;
        XUngrabPointer(QX11Info::display(), QX11Info::appTime());
        XSendEvent(QX11Info::display(), QX11Info::appRootWindow(info.screen()), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &xev);
        #else
        XUngrabPointer(QX11Info::display(), QX11Info::appTime());
        NETRootInfo rootInfo(QX11Info::display(), NET::WMMoveResize);
        rootInfo.moveResizeRequest(widget->window()->winId(), position.x(), position.y(), NET::Move);
        #endif // !ENABLE_KDE_SUPPORT
        #endif
    }

    if (!useWMMoveResize() && !_cursorOverride) {
        qApp->setOverrideCursor(Qt::SizeAllCursor);
        _cursorOverride = true;
    }

    _dragInProgress = true;
    return;
}

bool WindowManager::supportWMMoveResize(void) const
{
    #ifdef Q_WS_X11
    return true;
    #endif
    return false;
}

bool WindowManager::isDockWidgetTitle(const QWidget *widget) const
{
    if (!widget) {
        return false;
    }
    if (const QDockWidget *dockWidget = qobject_cast<const QDockWidget*>(widget->parent())) {
        return widget == dockWidget->titleBarWidget();
    } else {
       return false;
    }
}

bool WindowManager::AppEventFilter::eventFilter(QObject *object, QEvent *event)
{
    if (QEvent::MouseButtonRelease==event->type()) {
        // stop drag timer
        if (_parent->_dragTimer.isActive()) {
            _parent->resetDrag();
        }

        // unlock
        if (_parent->isLocked()) {
            _parent->setLocked(false);
        }
    }

    if (!_parent->enabled()) {
        return false;
    }

    /*
        if a drag is in progress, the widget will not receive any event
        we trigger on the first MouseMove or MousePress events that are received
        by any widget in the application to detect that the drag is finished
        */
    if (_parent->useWMMoveResize() && _parent->_dragInProgress && _parent->_target && (QEvent::MouseMove==event->type() || QEvent::MouseButtonPress==event->type())) {
        return appMouseEvent(object, event);
    }

    return false;
}

bool WindowManager::AppEventFilter::appMouseEvent(QObject *object, QEvent *event)
{
    Q_UNUSED(object);

    // store target window (see later)
    QWidget *window(_parent->_target.data()->window());

    /*
        post some mouseRelease event to the target, in order to counter balance
        the mouse press that triggered the drag. Note that it triggers a resetDrag
        */
    QMouseEvent mouseEvent(QEvent::MouseButtonRelease, _parent->_dragPoint, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    qApp->sendEvent(_parent->_target.data(), &mouseEvent);

    if (QEvent::MouseMove==event->type()) {
        /*
            HACK: quickly move the main cursor out of the window and back
            this is needed to get the focus right for the window children
            the origin of this issue is unknown at the moment
            */
        const QPoint cursor = QCursor::pos();
        QCursor::setPos(window->mapToGlobal(window->rect().topRight()) + QPoint(1, 0));
        QCursor::setPos(cursor);

    }
    return true;
}
