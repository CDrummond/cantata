/*
 * Cantata
 *
 * Copyright (c) 2011-2018 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "fancytabwidget.h"
#include "icon.h"
#include "action.h"
#include "utils.h"
#include "config.h"
#ifdef Q_OS_MAC
#include "osxstyle.h"
#endif
#include <QStyleOption>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QPainter>
#include <QSplitter>
#include <QStackedWidget>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QApplication>
#include <QSignalMapper>
#include <QMenu>

static inline int sidebarSpacing(bool withText)
{
    int sp=Utils::scaleForDpi(12);
    if (!withText) {
        sp*=1.25;
    }
    return sp;
}

static inline Qt::TextElideMode elideMode()
{
    return Qt::LeftToRight==QApplication::layoutDirection() ? Qt::ElideRight : Qt::ElideLeft;
}

static int largeIconSize=32;
static int smallIconSize=16;
void FancyTabWidget::setup()
{
    largeIconSize=Icon::stdSize(Utils::scaleForDpi(32));
    smallIconSize=Icon::stdSize(Utils::scaleForDpi(16));
}

int FancyTabWidget::iconSize(bool large)
{
    return large ? largeIconSize : smallIconSize;
}

static void drawIcon(const QIcon &icon, const QRect &r, QPainter *p, const QSize &iconSize, bool selected)
{
    #ifdef Q_OS_WIN
    Q_UNUSED(selected);
    QPixmap px = icon.pixmap(iconSize, QIcon::Normal);
    #else
    QPixmap px = icon.pixmap(iconSize, selected ? QIcon::Selected : QIcon::Normal);
    #endif
    QSize layoutSize = px.size() / px.DEVICE_PIXEL_RATIO();
    p->drawPixmap(r.x()+(r.width()-layoutSize.width())/2.0, r.y()+(r.height()-layoutSize.height())/2.0, layoutSize.width(), layoutSize.height(), px);
}

void FancyTabProxyStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *p, const QWidget *widget) const
{
    if (PE_FrameTabBarBase!=element) {
        QProxyStyle::drawPrimitive(element, option, p, widget);
    }
}

int FancyTabProxyStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget, QStyleHintReturn *returnData) const
{
    if (SH_TabBar_Alignment==hint && widget && qobject_cast<const QTabBar *>(widget)) {
        QTabBar::Shape shape=static_cast<const QTabBar *>(widget)->shape();
        if (QTabBar::RoundedNorth==shape || QTabBar::RoundedSouth==shape) {
            return (int)Qt::AlignCenter;
        }
    }
    return QProxyStyle::styleHint(hint, option, widget, returnData);
}

void FancyTabProxyStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *p, const QWidget *widget) const
{
    const QStyleOptionTab *tabOpt = qstyleoption_cast<const QStyleOptionTab*>(option);

    if (element != CE_TabBarTab || !tabOpt) {
        QProxyStyle::drawControl(element, option, p, widget);
        return;
    }

    const QRect rect = tabOpt->rect;
    const bool selected = tabOpt->state&State_Selected;
    const bool underMouse = tabOpt->state&State_MouseOver;
    bool active = tabOpt->state&State_Active;
    const bool verticalTabs = tabOpt->shape == QTabBar::RoundedWest;
    const QString text = tabOpt->text;

    QTransform m;
    if (verticalTabs) {
        m = QTransform::fromTranslate(rect.left(), rect.bottom());
        m.rotate(-90);
    } else {
        m = QTransform::fromTranslate(rect.left(), rect.top());
    }

    const QRect draw_rect(QPoint(0, 0), m.mapRect(rect).size());

    p->save();
    p->setTransform(m);

    QRect iconRect(QPoint(8, 0), tabOpt->iconSize);
    QRect textRect(iconRect.topRight() + QPoint(4, 0), draw_rect.size());
    textRect.setRight(draw_rect.width());
    iconRect.translate(0, (draw_rect.height() - iconRect.height()) / 2);

    if (selected || underMouse) {
        #ifdef Q_OS_MAC
        QColor col = OSXStyle::self()->viewPalette().highlight().color();
        #elif defined Q_OS_WIN
        QColor col = option->palette.highlight().color();
        col.setAlphaF(0.25);
        #else
        QColor col = option->palette.highlight().color();
        #endif

        if (selected) {
            #ifndef Q_OS_MAC
            if (!active && option->palette.highlight().color()==QApplication::palette().color(QPalette::Active, QPalette::Highlight)) {
                active = true;
            }
            #endif
        } else {
            #if defined Q_OS_WIN
            col.setAlphaF(0.1);
            #else
            col.setAlphaF(0.2);
            #endif
        }
        p->fillRect(draw_rect, col);
    }

    int textFlags = Qt::AlignTop | Qt::AlignVCenter;
    #ifdef Q_OS_MAC
    p->setPen(selected && active ? OSXStyle::self()->viewPalette().highlightedText().color() : OSXStyle::self()->viewPalette().foreground().color());
    #elif defined Q_OS_WIN
    p->setPen(QApplication::palette().foreground().color());
    #else
    p->setPen(selected && active ? QApplication::palette().highlightedText().color() : QApplication::palette().foreground().color());
    #endif

    drawIcon(tabOpt->icon, iconRect, p, tabOpt->iconSize, selected && active);

    QString txt=text;
    txt.replace("&", "");
    txt=p->fontMetrics().elidedText(txt, elideMode(), textRect.width());
    p->drawText(textRect.translated(0, -1), textFlags, txt);
    p->restore();
}

void FancyTabProxyStyle::polish(QWidget* widget)
{
    if (QLatin1String("QTabBar")==QString(widget->metaObject()->className())) {
        widget->setMouseTracking(true);
        widget->installEventFilter(this);
    }
    QProxyStyle::polish(widget);
}

void FancyTabProxyStyle::polish(QApplication* app)
{
    QProxyStyle::polish(app);
}

void FancyTabProxyStyle::polish(QPalette &palette)
{
    QProxyStyle::polish(palette);
}

bool FancyTabProxyStyle::eventFilter(QObject* o, QEvent* e)
{
    #ifndef Q_OS_MAC
    QTabBar *bar = qobject_cast<QTabBar*>(o);
    if (bar && (e->type() == QEvent::MouseMove || e->type() == QEvent::Leave)) {
        QMouseEvent *event = static_cast<QMouseEvent*>(e);
        const QString oldHoveredTab = bar->property("tab_hover").toString();
        const QString hoveredTab = e->type() == QEvent::Leave ? QString() : bar->tabText(bar->tabAt(event->pos()));
        bar->setProperty("tab_hover", hoveredTab);

        if (oldHoveredTab != hoveredTab) {
            bar->update();
        }
    }
    #endif

    return false;
}

FancyTab::FancyTab(FancyTabBar* tabbar)
    : QWidget(tabbar), tabbar(tabbar), underMouse(false)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
    setAttribute(Qt::WA_Hover, true);
}

FancyTabBar::FancyTabBar(QWidget *parent, bool text, int iSize, Pos pos)
    : QWidget(parent)
    , withText(text)
    , pos(pos)
    , icnSize(iSize)
{
    setFont(Utils::smallFont(font()));
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true); // Needed for hover events
    triggerTimer.setSingleShot(true);

    QBoxLayout* layout=nullptr;

    if (Side!=pos) {
        setMinimumHeight(tabSizeHint().height());
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout=new QHBoxLayout;
    } else {
        setMinimumWidth(tabSizeHint().width());
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        layout=new QVBoxLayout;
        layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::MinimumExpanding));
    }

    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    // We use a zerotimer to keep the sidebar responsive
    connect(&triggerTimer, SIGNAL(timeout()), this, SLOT(emitCurrentIndex()));
}

FancyTabBar::~FancyTabBar()
{
}

QSize FancyTab::sizeHint() const
{
    int iconSize = tabbar->iconSize();
    bool withText = tabbar->showText();
    int spacing = sidebarSpacing(withText);
    int padding = FancyTabBar::Side==tabbar->position() ? Utils::scaleForDpi(12) : 0;
    if (withText) {
        QFontMetrics fm(font());
        int textWidth = fm.width(text)*1.1;
        int width = qMax(iconSize, qMin(3*iconSize, textWidth)) + spacing;
        return QSize(width, iconSize + spacing + fm.height() + padding);
    } else {
        return QSize(iconSize + spacing + padding, iconSize + spacing + padding);
    }
}

QSize FancyTabBar::tabSizeHint() const
{
    int spacing = sidebarSpacing(withText);
    int padding = Side==pos ? Utils::scaleForDpi(12) : 0;
    if (withText) {
        QFontMetrics fm(font());
        int maxTw=0;
        for (FancyTab *tab: tabs) {
            maxTw=qMax(maxTw, tab->sizeHint().width());
        }
        return QSize(qMax(icnSize + spacing, maxTw), icnSize + spacing + fm.height() + padding);
    } else {
        return QSize(icnSize + spacing, icnSize + spacing + padding);
    }
}

void FancyTabBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);

    for (int i = 0; i < count(); ++i) {
        if (i != currentIndex()) {
            paintTab(&p, i);
        }
    }

    // paint active tab last, since it overlaps the neighbors
    if (currentIndex() != -1) {
        paintTab(&p, currentIndex());
    }
}

void FancyTab::enterEvent(QEvent*)
{
    underMouse = true;
}

void FancyTab::leaveEvent(QEvent*)
{
    underMouse = false;
}

QSize FancyTabBar::sizeHint() const
{
    QSize sh = tabSizeHint();
    return Side!=pos
            ? QSize(sh.width() * tabs.count(), sh.height())
            : QSize(sh.width(), sh.height() * tabs.count());
}

QSize FancyTabBar::minimumSizeHint() const
{
    QSize sh = tabSizeHint();
    return Side!=pos
            ? QSize(sh.width() * tabs.count(), sh.height())
            : QSize(sh.width(), sh.height() * tabs.count());
}

QRect FancyTabBar::tabRect(int index) const
{
    return tabs[index]->geometry();
}

QString FancyTabBar::tabToolTip(int index) const
{
    return tabs[index]->toolTip();
}

void FancyTabBar::setTabToolTip(int index, const QString &toolTip)
{
    tabs[index]->setToolTip(toolTip);
}

// This keeps the sidebar responsive since
// we get a repaint before loading the
// mode itself
void FancyTabBar::emitCurrentIndex()
{
    emit currentChanged(currentIdx);
}

void FancyTabBar::mousePressEvent(QMouseEvent *e)
{
    if (Qt::LeftButton!=e->button()) {
        return;
    }
    e->accept();
    for (int index = 0; index < tabs.count(); ++index) {
        if (tabRect(index).contains(e->pos())) {
            currentIdx = index;
            update();
            triggerTimer.start(0);
            break;
        }
    }
}

void FancyTabBar::wheelEvent(QWheelEvent *ev)
{
    int numDegrees = ev->delta() / 8;
    int numSteps = numDegrees / -15;
    int prevIndex = currentIdx;

    if (numSteps>0) {
        currentIdx=qMin(currentIdx+numSteps, tabs.size()-1);
    } else if (numSteps<0) {
        currentIdx=qMax(currentIdx+numSteps, 0);
    }
    if (currentIdx!=prevIndex) {
        update();
        triggerTimer.start(0);
    }
    ev->accept();
}

void FancyTabBar::addTab(const QIcon &icon, const QString &label, const QString &tt)
{
    FancyTab *tab = new FancyTab(this);
    tab->icon = icon;
    tab->text = label;
    tabs.append(tab);
    if (!tt.isEmpty()) {
        tab->setToolTip(tt);
    } else if (!withText) {
        tab->setToolTip(label);
    }
    qobject_cast<QBoxLayout*>(layout())->insertWidget(layout()->count()-(Side==pos ? 1 : 0), tab);
}

void FancyTabBar::addSpacer(int size)
{
    qobject_cast<QVBoxLayout*>(layout())->insertSpacerItem(layout()->count()-1,
                                                           new QSpacerItem(0, size, QSizePolicy::Fixed, QSizePolicy::Maximum));
}

void FancyTabBar::paintTab(QPainter *painter, int tabIndex) const
{
    if (!validIndex(tabIndex)) {
        qWarning("invalid index");
        return;
    }
    painter->save();

    QRect rect = tabRect(tabIndex);
    bool selected = (tabIndex == currentIdx);
    bool underMouse = tabs[tabIndex]->underMouse;

    if (selected || underMouse) {
        #ifdef Q_OS_MAC
        QColor col = OSXStyle::self()->viewPalette().highlight().color();
        #elif defined Q_OS_WIN
        QColor col = palette().highlight().color();
        col.setAlphaF(0.25);
        #else
        QColor col = palette().highlight().color();
        #endif

        if (!selected) {
            #if defined Q_OS_WIN
            col.setAlphaF(0.1);
            #else
            col.setAlphaF(0.2);
            #endif
        }
        painter->fillRect(rect, col);
    }

    selected = selected && (palette().currentColorGroup()==QPalette::Active ||
                           (palette().highlightedText().color()==palette().brush(QPalette::Active, QPalette::HighlightedText).color()));

    if (withText) {
        QString tabText(painter->fontMetrics().elidedText(this->tabText(tabIndex), elideMode(), width()));
        QRect tabTextRect(tabRect(tabIndex));
        QRect tabIconRect(tabTextRect);
        tabIconRect.adjust(+4, +4, -4, -4);
        tabTextRect.translate(0, -2);

        #ifdef Q_OS_MAC
        painter->setPen(selected ? OSXStyle::self()->viewPalette().highlightedText().color() : OSXStyle::self()->viewPalette().foreground().color());
        #elif defined Q_OS_WIN
        painter->setPen(QApplication::palette().foreground().color());
        #else
        painter->setPen(selected ? QApplication::palette().highlightedText().color() : QApplication::palette().foreground().color());
        #endif
        int textFlags = Qt::AlignCenter | Qt::AlignBottom;
        painter->drawText(tabTextRect, textFlags, tabText);

        const int textHeight = painter->fontMetrics().height();
        tabIconRect.adjust(0, 4, 0, -textHeight);
        drawIcon(tabIcon(tabIndex), tabIconRect, painter, QSize(icnSize, icnSize), selected);
    } else {
        drawIcon(tabIcon(tabIndex), rect, painter, QSize(icnSize, icnSize), selected);
    }

    painter->restore();
}

void FancyTabBar::setCurrentIndex(int index)
{
    currentIdx = index;
    update();
    emit currentChanged(currentIdx);
}

FancyTabWidget::FancyTabWidget(QWidget *parent)
    : QWidget(parent)
    , styleSetting(0)
    , tabBar(nullptr)
    , stack_(new QStackedWidget(this))
    , sideWidget(new QWidget)
    , sideLayout(new QVBoxLayout)
    , topLayout(new QVBoxLayout)
    , menu(nullptr)
    , proxyStyle(new FancyTabProxyStyle)
{
    sideLayout->setSpacing(0);
    sideLayout->setMargin(0);
    sideLayout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));

    sideWidget->setLayout(sideLayout);
    sideWidget->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

    topLayout->setMargin(0);
    topLayout->setSpacing(0);
    topLayout->addWidget(stack_);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->setMargin(0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(sideWidget);
    mainLayout->addLayout(topLayout);
    setLayout(mainLayout);
}

void FancyTabWidget::addTab(QWidget *tab, const QIcon &icon, const QString &label, const QString &tt, bool enabled)
{
    stack_->addWidget(tab);
    items << Item(icon, label, tt, enabled);
    setMinimumWidth(128);
}

int FancyTabWidget::currentIndex() const
{
    return stack_->currentIndex();
}

QWidget * FancyTabWidget::currentWidget() const
{
    return stack_->currentWidget();
}

QWidget * FancyTabWidget::widget(int index) const
{
    return stack_->widget(index);
}

int FancyTabWidget::count() const
{
    return stack_->count();
}

int FancyTabWidget::visibleCount() const
{
    int c=0;
    for (const Item &i: items) {
        if (i.enabled) {
            c++;
        }
    }
    return c;
}

QSize FancyTabWidget::tabSize() const
{
    if (FancyTabBar *bar = qobject_cast<FancyTabBar*>(tabBar)) {
        return bar->tabSizeHint();
    }
    return QSize();
}

void FancyTabWidget::setCurrentIndex(int idx)
{
    if (!isEnabled(idx)) {
        return;
    }
    int index=IndexToTab(idx);
    if (FancyTabBar* bar = qobject_cast<FancyTabBar*>(tabBar)) {
        bar->setCurrentIndex(index);
    } else if (QTabBar* bar = qobject_cast<QTabBar*>(tabBar)) {
        bar->setCurrentIndex(index);
        showWidget(index);
    } else {
        stack_->setCurrentIndex(idx); // ?? IS this *ever* called???
    }
}

void FancyTabWidget::showWidget(int index)
{
    int idx=tabToIndex(index);
    stack_->setCurrentIndex(idx);
    emit currentChanged(idx);
}

void FancyTabWidget::setStyle(int s)
{
    if (s==styleSetting && tabBar) {
        return;
    }
    // Remove previous tab bar
    delete tabBar;
    tabBar = nullptr;

    //   use_background_ = false;
    // Create new tab bar
    if (Tab==(s&Style_Mask) || (Small==(s&Style_Mask) && !(s&IconOnly))) {
        switch (s&Position_Mask) {
        default:
        case Side:
            makeTabBar(QApplication::isRightToLeft() ? QTabBar::RoundedEast : QTabBar::RoundedWest, !(s&IconOnly), true, Small==(s&Style_Mask));
            break;
        case Top:
            makeTabBar(QTabBar::RoundedNorth, !(s&IconOnly), true, Small==(s&Style_Mask)); break;
        case Bot:
            makeTabBar(QTabBar::RoundedSouth, !(s&IconOnly), true, Small==(s&Style_Mask)); break;
        }
    } else {
        FancyTabBar* bar = new FancyTabBar(this, !(s&IconOnly), Small==(s&Style_Mask) ? smallIconSize : largeIconSize,
                                           Side==(s&Position_Mask) ? FancyTabBar::Side : (Top==(s&Position_Mask) ? FancyTabBar::Top : FancyTabBar::Bot));
        switch (s&Position_Mask) {
        default:
        case Side: sideLayout->insertWidget(0, bar); break;
        case Bot:  topLayout->insertWidget(1, bar); break;
        case Top:  topLayout->insertWidget(0, bar); break;
        }

        tabBar = bar;

        int index=0;
        QList<Item>::Iterator it=items.begin();
        QList<Item>::Iterator end=items.end();
        for (; it!=end; ++it) {
            if (!(*it).enabled) {
                (*it).index=-1;
                continue;
            }
            if (Item::Type_Spacer==(*it).type) {
                bar->addSpacer((*it).spacerSize);
            } else {
                bar->addTab((*it).tabIcon, (*it).tabLabel, (*it).tabTooltip);
            }
            (*it).index=index++;
        }

        bar->setCurrentIndex(IndexToTab(stack_->currentIndex()));
        connect(bar, SIGNAL(currentChanged(int)), SLOT(showWidget(int)));
    }
    tabBar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    styleSetting = s;
    emit styleChanged(s);
    update();
}

void FancyTabWidget::toggleTab(int tab, bool show)
{
    if (tab>=0 && tab<items.count()) {
        items[tab].enabled=show;
        recreate();
        emit tabToggled(tab);
    }
}

void FancyTabWidget::makeTabBar(QTabBar::Shape shape, bool text, bool icons, bool fancy)
{
    QTabBar *bar = new QTabBar(this);
    bar->setShape(shape);
    bar->setDocumentMode(true);
    bar->setUsesScrollButtons(true);

    if (QTabBar::RoundedWest==shape) {
        bar->setIconSize(QSize(smallIconSize, smallIconSize));
    }

    if (fancy) {
        bar->setStyle(proxyStyle.data());
    }

    if (QTabBar::RoundedNorth==shape) {
        topLayout->insertWidget(0, bar);
    } else if (QTabBar::RoundedSouth==shape) {
        topLayout->insertWidget(1, bar);
    } else {
        sideLayout->insertWidget(0, bar);
    }

    int index=0;
    QList<Item>::Iterator it=items.begin();
    QList<Item>::Iterator end=items.end();
    for (; it!=end; ++it) {
        if ((*it).type != Item::Type_Tab || !(*it).enabled) {
            (*it).index=-1;
            continue;
        }

        QString label = (*it).tabLabel;
        if (QTabBar::RoundedWest==shape) {
            label = QFontMetrics(font()).elidedText(label, elideMode(), 120);
        }

        int tabId = -1;
        if (icons && text) {
            tabId = bar->addTab((*it).tabIcon, label);
        } else if (icons) {
            tabId = bar->addTab((*it).tabIcon, QString());
        } else if (text) {
            tabId = bar->addTab(label);
        }

        if (!(*it).tabTooltip.isEmpty()) {
            bar->setTabToolTip(tabId, (*it).tabTooltip);
        } else if (!text) {
            bar->setTabToolTip(tabId, (*it).tabLabel);
        }
        (*it).index=index++;
    }

    bar->setCurrentIndex(IndexToTab(stack_->currentIndex()));
    connect(bar, SIGNAL(currentChanged(int)), SLOT(showWidget(int)));
    tabBar = bar;
}

int FancyTabWidget::tabToIndex(int tab) const
{
    for (int i=0; i<items.count(); ++i) {
        if (items[i].index==tab) {
            return i;
        }
    }

    return 0;
}

void FancyTabWidget::setIcon(int index, const QIcon &icon)
{
    if (index>-1 && index<items.count()) {
        items[index].tabIcon=icon;
    }
}

void FancyTabWidget::setToolTip(int index, const QString &tt)
{
    if (index>-1 && index<items.count()) {
        Item &item=items[index];
        item.tabTooltip=tt.isEmpty() ? item.tabLabel : tt;

        if (tabBar && -1!=item.index) {
            if (qobject_cast<QTabBar *>(tabBar)) {
                static_cast<QTabBar *>(tabBar)->setTabToolTip(item.index, item.tabTooltip);
            } else {
                static_cast<FancyTabBar *>(tabBar)->setTabToolTip(item.index, item.tabTooltip);
            }
        }
    }
}

void FancyTabWidget::recreate()
{
    int s=styleSetting;
    styleSetting=0;
    setStyle(s);
}

void FancyTabWidget::setHiddenPages(const QStringList &hidden)
{
    QSet<QString> h=hidden.toSet();
    if (h==hiddenPages().toSet()) {
        return;
    }
    bool needToRecreate=false;
    bool needToSetCurrent=false;
    for (int i=0; i<count(); ++i) {
        QWidget *w=widget(i);
        if (w && items[i].enabled==hidden.contains(w->metaObject()->className())) {
            items[i].enabled=!items[i].enabled;
            emit tabToggled(i);
            needToRecreate=true;
            if (i==currentIndex()) {
                needToSetCurrent=true;
            }
        }
    }
    if (needToRecreate) {
        recreate();
    }
    if (needToSetCurrent) {
        for (int i=0; i<count(); ++i) {
            QWidget *w=widget(i);
            if (w && items[i].enabled) {
                setCurrentIndex(i);
                break;
            }
        }
    }
}

QStringList FancyTabWidget::hiddenPages() const
{
    QStringList pages;
    for (int i=0; i<count(); ++i) {
        if (!isEnabled(i)) {
            QWidget *w=widget(i);
            if (w) {
                pages << w->metaObject()->className();
            }
        }
    }
    return pages;
}

#include "moc_fancytabwidget.cpp"
