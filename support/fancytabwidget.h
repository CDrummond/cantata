/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
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
#include <QTimer>
#include <QScopedPointer>
#include <QStringList>

class QMenu;
class QPainter;
class QSignalMapper;
class QStackedWidget;
class QStatusBar;
class QVBoxLayout;

class FancyTabProxyStyle : public QProxyStyle
{
    Q_OBJECT

public:
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *p, const QWidget *widget) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *p, const QWidget *widget) const override;
    void polish(QWidget *widget) override;
    void polish(QApplication *app) override;
    void polish(QPalette &palette) override;

protected:
    bool eventFilter(QObject *o, QEvent *e) override;
};

class FancyTabBar;
class FancyTab : public QWidget
{
public:
    FancyTab(FancyTabBar *tabbar);

    QSize sizeHint() const override;
    QIcon icon;
    QString text;
    bool underMouse;

protected:
    void enterEvent(QEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    FancyTabBar *tabbar;
};

class FancyTabBar : public QWidget
{
    Q_OBJECT

public:
    enum Pos {
        Side,
        Top,
        Bot
    };

    FancyTabBar(QWidget *parent, bool text, int iSize, Pos pos);
    ~FancyTabBar() override;

    void paintEvent(QPaintEvent *event) override;
    void paintTab(QPainter *painter, int tabIndex) const;
    void mousePressEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *ev) override;
    bool validIndex(int index) const { return index >= 0 && index < tabs.count(); }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    void addTab(const QIcon &icon, const QString &label, const QString &tt);
    void addSpacer(int size = 40);
    void removeTab(int index) { delete tabs.takeAt(index); }
    void setCurrentIndex(int index);
    int currentIndex() const { return currentIdx; }

    void setTabToolTip(int index, const QString &toolTip);
    QString tabToolTip(int index) const;

    QIcon tabIcon(int index) const {return tabs.at(index)->icon; }
    QString tabText(int index) const { return tabs.at(index)->text; }
    int count() const {return tabs.count(); }
    QRect tabRect(int index) const;
    bool showText() const { return withText; }
    int iconSize() const { return icnSize; }
    QSize tabSizeHint() const;
    Pos position() const { return pos; }

Q_SIGNALS:
    void currentChanged(int);

public Q_SLOTS:
    void emitCurrentIndex();

private:
    int currentIdx;
    QList<FancyTab*> tabs;
    QTimer triggerTimer;
    bool withText : 1;
    Pos pos : 2;
    int icnSize;
};

class FancyTabWidget : public QWidget
{
    Q_OBJECT

public:
    static void setup();
    static int iconSize(bool large=true);

    FancyTabWidget(QWidget *parent);

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
        Item(const QIcon &icon, const QString &label, const QString &tt, bool en)
            : type(Type_Tab), tabLabel(label), tabTooltip(tt), tabIcon(icon), spacerSize(0), enabled(en), index(-1) {}
        Item(int size) : type(Type_Spacer), spacerSize(size), enabled(true), index(-1) {}

        enum Type {
            Type_Tab,
            Type_Spacer
        };

        Type type;
        QString tabLabel;
        QString tabTooltip;
        QIcon tabIcon;
        int spacerSize;
        bool enabled;
        int index;
    };

    void addTab(QWidget *tab, const QIcon &icon, const QString &label, const QString &tt=QString(), bool enabled=true);
    int currentIndex() const;
    QWidget * currentWidget() const;
    bool isEnabled(int index) const { return index>=0 && index<items.count() ? items[index].enabled : false; }
    QWidget * widget(int index) const;
    int count() const;
    int visibleCount() const;
    int style() const { return styleSetting; }
    QSize tabSize() const;
    void setIcon(int index, const QIcon &icon);
    void setToolTip(int index, const QString &tt);
    void recreate();
    void setHiddenPages(const QStringList &hidden);
    QStringList hiddenPages() const;

public Q_SLOTS:
    void setCurrentIndex(int index);
    void setStyle(int s);
    void toggleTab(int tab, bool show);

Q_SIGNALS:
    void currentChanged(int index);
    void styleChanged(int styleSetting);
    void tabToggled(int index);

private Q_SLOTS:
    void showWidget(int index);

private:
    void makeTabBar(QTabBar::Shape shape, bool text, bool icons, bool fancy);
    int tabToIndex(int tab) const;
    int IndexToTab(int index) const { return index>=0 && index<items.count() ? items[index].index : 0; }

private:
    int styleSetting;
    QList<Item> items;
    QWidget *tabBar;
    QStackedWidget *stack_;
    QWidget *sideWidget;
    QVBoxLayout *sideLayout;
    QVBoxLayout *topLayout;
    QMenu *menu;
    QScopedPointer<FancyTabProxyStyle> proxyStyle;
};


#endif // FANCYTABWIDGET_H
