#ifndef __WINDOW_MANAGER_H__
#define __WINDOW_MANAGER_H__

/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */

// Copied from oxygenwindowmanager.h svnversion: 1137195

//////////////////////////////////////////////////////////////////////////////
// oxygenwindowmanager.h
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

#include <QEvent>
#include <QBasicTimer>
#include <QObject>
#include <QSet>
#include <QString>
#include <QWeakPointer>
#include <QWidget>

class WindowManager: public QObject
{
    Q_OBJECT
    
    #if QT_VERSION < 0x040600
    class Pointer : public QObject
    {
    public:
        Pointer(QWidget *w=0L) : widget_(w) {}
        Pointer & operator=(QWidget *w);
        operator bool() const { return 0L!=widget_; }
        void clear();
        bool eventFilter(QObject *, QEvent *);
        QWidget *data() { return widget_; }

    private:
        QWidget *widget_;
    };
    #endif
    
public:

    enum DragMode
    {
        WM_DRAG_NONE             = 0,
        WM_DRAG_MENUBAR          = 1,
        WM_DRAG_MENU_AND_TOOLBAR = 2,
        WM_DRAG_ALL              = 3
    };

    explicit WindowManager(QObject *);
    virtual ~WindowManager() { }

    void initialize(int windowDrag);
    void registerWidgetAndChildren(QWidget *w);
    void registerWidget(QWidget *);
    void unregisterWidget(QWidget *);
    virtual bool eventFilter(QObject *, QEvent *);

protected:
    //! timer event, used to start drag if button is pressed for a long enough time */
    void timerEvent(QTimerEvent *);

    bool mousePressEvent(QObject *, QEvent *);
    bool mouseMoveEvent(QObject *, QEvent *);
    bool mouseReleaseEvent(QObject *, QEvent *);
    bool enabled(void) const { return WM_DRAG_NONE!=_dragMode; }

    //! returns true if window manager is used for moving
    bool useWMMoveResize(void) const { return supportWMMoveResize() && _useWMMoveResize; }

    //! use window manager for moving, when available
    void setUseWMMoveResize(bool value) { _useWMMoveResize = value; }

    int dragMode(void) const { return _dragMode; }
    void setDragMode(int value) { _dragMode = value; }

    //! drag distance (pixels)
    void setDragDistance(int value)  { _dragDistance = value; }

    //! drag delay (msec)
    void setDragDelay(int value)  { _dragDelay = value; }

    //! returns true if widget is dragable
    bool isDragable(QWidget *);

    //! returns true if widget is dragable
    bool isBlackListed(QWidget *);

    //! returns true if drag can be started from current widget
    bool canDrag(QWidget *);

    //! returns true if drag can be started from current widget and position
    /*! child at given position is passed as second argument */
    bool canDrag(QWidget *, QWidget *, const QPoint &);

    //! reset drag
    void resetDrag(void);

    //! start drag
    void startDrag(QWidget *, const QPoint &);

    //! returns true if window manager is used for moving
    /*! right now this is true only for X11 */
    bool supportWMMoveResize(void) const;

    //! utility function
    bool isDockWidgetTitle(const QWidget *) const;

    void setLocked(bool value) { _locked = value; }
    bool isLocked(void) const  { return _locked; }


private:
    bool _useWMMoveResize;
    int _dragMode;
    int _dragDistance;
    int _dragDelay;

    //! drag point
    QPoint _dragPoint;
    QPoint _globalDragPoint;

    //! drag timer
    QBasicTimer _dragTimer;

    //! target being dragged
    /*! QWeakPointer is used in case the target gets deleted while drag is in progress */
    #if QT_VERSION < 0x040600
    Pointer _target;
    #else
    QWeakPointer<QWidget> _target;
    #endif

    //! true if drag is about to start
    bool _dragAboutToStart;

    //! true if drag is in progress
    bool _dragInProgress;

    //! true if drag is locked
    bool _locked;

    //! cursor override
    /*! used to keep track of application cursor being overridden when dragging in non-WM mode */
    bool _cursorOverride;

    // provide application-wise event filter
    // it us used to unlock dragging and make sure event look is properly restored
    // after a drag has occurred
    class AppEventFilter: public QObject
    {
    public:

        //! constructor
        AppEventFilter(WindowManager *parent)
            : QObject(parent)
            , _parent(parent) {
        }

        virtual bool eventFilter(QObject *, QEvent *);

    protected:
        //! application-wise event. needed to catch end of XMoveResize events */
        bool appMouseEvent(QObject *, QEvent *);

    private:
        WindowManager *_parent;
    };

    AppEventFilter *_appEventFilter;
    friend class AppEventFilter;
};
#endif
