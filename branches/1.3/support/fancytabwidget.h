/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
 *
 */
/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2010 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/

#ifndef FANCYTABWIDGET_H
#define FANCYTABWIDGET_H

#include <QIcon>
#include <QProxyStyle>
#include <QTabBar>
#include <QWidget>
#include <QPropertyAnimation>
#include <QTimer>
#include <QScopedPointer>
#include <QStringList>

//#include <boost/scoped_ptr.hpp>

class QMenu;
class QPainter;
class QSignalMapper;
class QStackedWidget;
class QStatusBar;
class QVBoxLayout;

namespace Core {
namespace Internal {

class FancyTabProxyStyle : public QProxyStyle {
    Q_OBJECT

public:
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *p, const QWidget *widget) const;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *p, const QWidget *widget) const;
    void polish(QWidget* widget);
    void polish(QApplication* app);
    void polish(QPalette& palette);

protected:
    bool eventFilter(QObject* o, QEvent* e);
};

class FancyTabBar;
class FancyTab : public QWidget{
    Q_OBJECT

    Q_PROPERTY(float fader READ fader WRITE setFader)
public:
    FancyTab(FancyTabBar *tabbar);
    float fader() { return m_fader; }
    void setFader(float value);

    QSize sizeHint() const;

    void fadeIn();
    void fadeOut();

    QIcon icon;
    QString text;

protected:
    void enterEvent(QEvent *);
    void leaveEvent(QEvent *);

private:
    QPropertyAnimation animator;
    FancyTabBar *tabbar;
    float m_fader;
};

class FancyTabBar : public QWidget
{
    Q_OBJECT

public:
    enum Pos {
        Side, Top, Bot
    };

    FancyTabBar(QWidget *parent, bool hasBorder, bool text, int iSize, Pos pos);
    ~FancyTabBar();

    void paintEvent(QPaintEvent *event);
    void paintTab(QPainter *painter, int tabIndex, bool gtkStyle) const;
    void mousePressEvent(QMouseEvent *);
    bool validIndex(int index) const { return index >= 0 && index < m_tabs.count(); }

    QSize sizeHint() const;
    QSize minimumSizeHint() const;

    void addTab(const QIcon &icon, const QString &label, const QString &tt);
    void addSpacer(int size = 40);
    void removeTab(int index) { delete m_tabs.takeAt(index); }
    void setCurrentIndex(int index);
    int currentIndex() const { return m_currentIndex; }

    void setTabToolTip(int index, const QString& toolTip);
    QString tabToolTip(int index) const;

    QIcon tabIcon(int index) const {return m_tabs.at(index)->icon; }
    QString tabText(int index) const { return m_tabs.at(index)->text; }
    int count() const {return m_tabs.count(); }
    QRect tabRect(int index) const;
    bool showText() const { return m_showText; }
    int iconSize() const { return m_iconSize; }
    QSize tabSizeHint() const;

Q_SIGNALS:
    void currentChanged(int);

public Q_SLOTS:
    void emitCurrentIndex();

private:
    static const int m_rounding;
    static const int m_textPadding;
    int m_currentIndex;
    QList<FancyTab*> m_tabs;
    QTimer m_triggerTimer;
    bool m_hasBorder : 1;
    bool m_showText : 1;
    Pos m_pos : 2;
    int m_iconSize;
};

class FancyTabWidget : public QWidget {
    Q_OBJECT

public:
    static void setup();
    static int iconSize(bool large=true);

    FancyTabWidget(QWidget *parent, bool allowContext=true, bool drawBorder=false);

    enum Style {
        Side     = 0x0001,
        Top      = 0x0002,
        Bot      = 0x0003,

        Large    = 0x0010,
        Small    = 0x0020,
        Tab      = 0x0030,

        IconOnly = 0x0100,

        Position_Mask = 0x000F,
        Style_Mask    = 0x00F0,
        Options_Mask  = 0x0100,
        All_Mask      = Position_Mask|Style_Mask|Options_Mask
    };

    struct Item {
        Item(const QIcon& icon, const QString& label, const QString &tt, bool enabled)
            : type_(Type_Tab), tab_label_(label), tab_tooltip_(tt), tab_icon_(icon), spacer_size_(0), enabled_(enabled), index_(-1) {}
        Item(int size) : type_(Type_Spacer), spacer_size_(size), enabled_(true), index_(-1) {}

        enum Type {
            Type_Tab,
            Type_Spacer
        };

        Type type_;
        QString tab_label_;
        QString tab_tooltip_;
        QIcon tab_icon_;
        int spacer_size_;
        bool enabled_;
        int index_;
    };

    void AddTab(QWidget *tab, const QIcon &icon, const QString &label, const QString &tt=QString(), bool enabled=true);
//    void InsertTab(QWidget *tab, const QIcon &icon, const QString &label, const QString &tt=QString(), bool enabled=true);
//    void RemoveTab(QWidget *tab);
//    int IndexOf(QWidget *tab);
//    void AddSpacer(int size = 40);
//    void SetBackgroundPixmap(const QPixmap& pixmap);
//    void AddBottomWidget(QWidget* widget);
    int current_index() const;
    QWidget * currentWidget() const;
    bool isEnabled(int index) const { return index>=0 && index<items_.count() ? items_[index].enabled_ : false; }
    QWidget * widget(int index) const;
    int count() const;
    int visibleCount() const;
    int style() const { return style_; }
    QSize tabSize() const;
    void SetIcon(int index, const QIcon &icon);
    void SetToolTip(int index, const QString &tt);
    void Recreate();
    void setHiddenPages(const QStringList &hidden);
    QStringList hiddenPages() const;

public Q_SLOTS:
    void SetCurrentIndex(int index);
    void setStyle(int s);
    void ToggleTab(int tab, bool show);

Q_SIGNALS:
    void CurrentChanged(int index);
    void styleChanged(int style);
    void TabToggled(int index);
    void configRequested();

protected:
    void paintEvent(QPaintEvent *event);

private Q_SLOTS:
    void ShowWidget(int index);

private:
    void contextMenuEvent(QContextMenuEvent* e);
    void MakeTabBar(QTabBar::Shape shape, bool text, bool icons, bool fancy);
    int TabToIndex(int tab) const;
    int IndexToTab(int index) const { return index>=0 && index<items_.count() ? items_[index].index_ : 0; }

    int style_;
    QList<Item> items_;

    QWidget* tab_bar_;
    QStackedWidget* stack_;
//    QPixmap background_pixmap_;
    QWidget* side_widget_;
    QVBoxLayout* side_layout_;
    QVBoxLayout* top_layout_;

    //   bool use_background_;

    QMenu* menu_;

    //boost::scoped_ptr<FancyTabProxyStyle> proxy_style_;
    QScopedPointer<FancyTabProxyStyle> proxy_style_;
    bool allowContext_;
    bool drawBorder_;
};

} // namespace Internal
} // namespace Core

Q_DECLARE_METATYPE(QPropertyAnimation*);

using Core::Internal::FancyTab;
using Core::Internal::FancyTabBar;
using Core::Internal::FancyTabWidget;

#endif // FANCYTABWIDGET_H
