/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "localize.h"
#include "icon.h"
#include "gtkstyle.h"
#include "action.h"
#include "utils.h"
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyleOption>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QApplication>
#include <QAnimationGroup>
#include <QPropertyAnimation>
#include <QSignalMapper>
#include <QMenu>

static inline int sidebarSpacing(bool withText)
{
    int sp=Utils::isHighDpi() ? 24 : 12;
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
    if (Utils::isHighDpi()) {
        largeIconSize=Icon::stdSize(40);
        smallIconSize=16;
        if (largeIconSize>32) {
            if (largeIconSize<56) {
                smallIconSize=22;
            } else {
                smallIconSize=32;
            }
        }
    } else {
        largeIconSize=32;
        smallIconSize=Icon::touchFriendly() ? 22 : 16;
    }
}

int FancyTabWidget::iconSize(bool large)
{
    return large ? largeIconSize : smallIconSize;
}

static void drawIcon(const QIcon &icon, const QRect &r, QPainter *p, const QSize &iconSize, bool selected)
{
    QPixmap px = icon.pixmap(iconSize, selected ? QIcon::Selected : QIcon::Normal);
    p->drawPixmap(r.x()+(r.width()-px.width())/2.0, r.y()+(r.height()-px.height())/2.0, px.width(), px.height(), px);
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

    const QStyleOptionTabV3 *v3Opt = qstyleoption_cast<const QStyleOptionTabV3*>(option);

    if (element != CE_TabBarTab || !v3Opt) {
        QProxyStyle::drawControl(element, option, p, widget);
        return;
    }

    const QRect rect = v3Opt->rect;
    const bool selected = v3Opt->state  &State_Selected;
    const bool verticalTabs = v3Opt->shape == QTabBar::RoundedWest;
    const QString text = v3Opt->text;

    QTransform m;
    if (verticalTabs) {
        m = QTransform::fromTranslate(rect.left(), rect.bottom());
        m.rotate(-90);
    } else {
        m = QTransform::fromTranslate(rect.left(), rect.top());
    }

    const QRect draw_rect(QPoint(0, 0), m.mapRect(rect).size());

    if (!selected && GtkStyle::isActive()) {
        p->fillRect(option->rect, option->palette.background());
    }

    p->save();
    p->setTransform(m);

    QRect iconRect(QPoint(8, 0), v3Opt->iconSize);
    QRect textRect(iconRect.topRight() + QPoint(4, 0), draw_rect.size());
    textRect.setRight(draw_rect.width());
    iconRect.translate(0, (draw_rect.height() - iconRect.height()) / 2);

    QStyleOptionViewItemV4 styleOpt;
    styleOpt.palette=option->palette;
    styleOpt.rect=draw_rect;
    if (QStyleOptionTab::Beginning==v3Opt->position) {
        styleOpt.rect.adjust(0, 0, -1, 0);
    }
    styleOpt.state=option->state;
    styleOpt.state&=~(QStyle::State_Selected|QStyle::State_MouseOver);
    styleOpt.state|=QStyle::State_Selected|QStyle::State_Enabled;
    styleOpt.viewItemPosition = QStyleOptionViewItemV4::OnlyOne;
    styleOpt.showDecorationSelected=true;
    bool drawBgnd=true;
    int fader = 1;

    if (!selected && drawBgnd) {
        const QString faderKey = "tab_" + text + "_fader";
        const QString animationKey = "tab_" + text + "_animation";

        const QString tab_hover = widget->property("tab_hover").toString();
        fader=widget->property(faderKey.toUtf8().constData()).toInt();
        QPropertyAnimation *animation = widget->property(animationKey.toUtf8().constData()).value<QPropertyAnimation*>();

        if (!animation) {
            QWidget* mut_widget = const_cast<QWidget*>(widget);
            fader = 0;
            mut_widget->setProperty(faderKey.toUtf8().constData(), fader);
            animation = new QPropertyAnimation(mut_widget, faderKey.toUtf8(), mut_widget);
            connect(animation, SIGNAL(valueChanged(QVariant)), mut_widget, SLOT(update()));
            mut_widget->setProperty(animationKey.toUtf8().constData(), QVariant::fromValue(animation));
        }

        if (text == tab_hover) {
            if (animation->state() != QAbstractAnimation::Running && fader != 40) {
                animation->stop();
                animation->setDuration(80);
                animation->setEndValue(50);
                animation->start();
            }
        } else {
            if (animation->state() != QAbstractAnimation::Running && fader != 0) {
                animation->stop();
                animation->setDuration(160);
                animation->setEndValue(0);
                animation->start();
            }
        }

        if (fader<1) {
            drawBgnd=false;
        } else {
            QColor col(styleOpt.palette.highlight().color());
            col.setAlpha(fader);
            styleOpt.palette.setColor(styleOpt.palette.currentColorGroup(), QPalette::Highlight, col);
        }
    }

    if (drawBgnd) {
        if (!selected && GtkStyle::isActive()) {
            GtkStyle::drawSelection(styleOpt, p, (fader*1.0)/150.0);
        } else {
            QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &styleOpt, p, 0);
        }
    }

    int textFlags = Qt::AlignTop | Qt::AlignVCenter;
    p->setPen(selected && option->state&State_Active
              ? QApplication::palette().highlightedText().color() : QApplication::palette().foreground().color());

    drawIcon(v3Opt->icon, iconRect, p, v3Opt->iconSize,
             selected && option->state&State_Active);

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

    return false;
}

FancyTab::FancyTab(FancyTabBar* tabbar)
    : QWidget(tabbar), tabbar(tabbar), faderValue(0)
{
    animator.setPropertyName("fader");
    animator.setTargetObject(this);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
}

void FancyTab::fadeIn()
{
    animator.stop();
    animator.setDuration(80);
    animator.setEndValue(50);
    animator.start();
}

void FancyTab::fadeOut()
{
    animator.stop();
    animator.setDuration(160);
    animator.setEndValue(0);
    animator.start();
}

void FancyTab::setFader(float value)
{
    faderValue = value;
    tabbar->update();
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

    QBoxLayout* layout=0;

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
    if (withText) {
        QFontMetrics fm(font());
        int textWidth = fm.width(text)*1.1;
        int width = qMax(iconSize, qMin(3*iconSize, textWidth)) + spacing;
        return QSize(width, iconSize + spacing + fm.height());
    } else {
        return QSize(iconSize + spacing, iconSize + spacing);
    }
}

QSize FancyTabBar::tabSizeHint() const
{
    int spacing = sidebarSpacing(withText);
    if (withText) {
        QFontMetrics fm(font());
        int maxTw=0;
        foreach (FancyTab *tab, tabs) {
            maxTw=qMax(maxTw, tab->sizeHint().width());
        }
        return QSize(qMax(icnSize + spacing, maxTw), icnSize + spacing + fm.height());
    } else {
        return QSize(icnSize + spacing, icnSize + spacing);
    }
}

void FancyTabBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    bool gtkStyle=GtkStyle::isActive();

    for (int i = 0; i < count(); ++i) {
        if (i != currentIndex()) {
            paintTab(&p, i, gtkStyle);
        }
    }

    // paint active tab last, since it overlaps the neighbors
    if (currentIndex() != -1) {
        paintTab(&p, currentIndex(), gtkStyle);
    }
}

void FancyTab::enterEvent(QEvent*)
{
    fadeIn();
}

void FancyTab::leaveEvent(QEvent*)
{
    fadeOut();
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

void FancyTabBar::paintTab(QPainter *painter, int tabIndex, bool gtkStyle) const
{
    if (!validIndex(tabIndex)) {
        qWarning("invalid index");
        return;
    }
    painter->save();

    QRect rect = tabRect(tabIndex);
    bool selected = (tabIndex == currentIdx);

    QStyleOptionViewItemV4 styleOpt;
    styleOpt.initFrom(this);
    styleOpt.state&=~(QStyle::State_Selected|QStyle::State_MouseOver);
    styleOpt.state|=QStyle::State_Selected|QStyle::State_Enabled;
    styleOpt.rect=rect;
    styleOpt.viewItemPosition = QStyleOptionViewItemV4::OnlyOne;
    styleOpt.showDecorationSelected=true;
    bool drawBgnd=true;

    if (!selected && drawBgnd) {
        int fader=int(tabs[tabIndex]->fader());
        if (fader<1) {
            drawBgnd=false;
        } else  {
            QColor col(styleOpt.palette.highlight().color());
            col.setAlpha(fader);
            styleOpt.palette.setColor(styleOpt.palette.currentColorGroup(), QPalette::Highlight, col);
        }
    }
    if (drawBgnd) {
        if (!selected && gtkStyle) {
            GtkStyle::drawSelection(styleOpt, painter, tabs[tabIndex]->fader()/150.0);
        } else {
            QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &styleOpt, painter, 0);
        }
    }

    if (withText) {
        QString tabText(painter->fontMetrics().elidedText(this->tabText(tabIndex), elideMode(), width()));
        QRect tabTextRect(tabRect(tabIndex));
        QRect tabIconRect(tabTextRect);
        tabIconRect.adjust(+4, +4, -4, -4);
        tabTextRect.translate(0, -2);
        painter->setPen(selected ? palette().highlightedText().color() : palette().foreground().color());
        int textFlags = Qt::AlignCenter | Qt::AlignBottom;
        painter->drawText(tabTextRect, textFlags, tabText);

        const int textHeight = painter->fontMetrics().height();
        tabIconRect.adjust(0, 4, 0, -textHeight);
        drawIcon(tabIcon(tabIndex), tabIconRect, painter, QSize(icnSize, icnSize),
                 selected && (palette().currentColorGroup()==QPalette::Active ||
                              (palette().highlightedText().color()==palette().brush(QPalette::Active, QPalette::HighlightedText).color())));
    } else {
        drawIcon(tabIcon(tabIndex), rect, painter, QSize(icnSize, icnSize),
                 selected && (palette().currentColorGroup()==QPalette::Active ||
                              (palette().highlightedText().color()==palette().brush(QPalette::Active, QPalette::HighlightedText).color())));
    }

    painter->restore();
}

void FancyTabBar::setCurrentIndex(int index)
{
    currentIdx = index;
    update();
    emit currentChanged(currentIdx);
}

FancyTabWidget::FancyTabWidget(QWidget *parent, bool allowContextMenu)
    : QWidget(parent)
    , styleSetting(0)
    , tabBar(NULL)
    , stack_(new QStackedWidget(this))
    , sideWidget(new QWidget)
    , sideLayout(new QVBoxLayout)
    , topLayout(new QVBoxLayout)
    , menu(0)
    , proxyStyle(new FancyTabProxyStyle)
    , allowContext(allowContextMenu)
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
    setStyle(Side|Large);
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
    foreach (const Item &i, items) {
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

void FancyTabWidget::contextMenuEvent(QContextMenuEvent *e)
{
    if (!allowContext) {
        return;
    }

    // Check we are over tab space...
    if (Tab==(styleSetting&Style_Mask)) {
        if (QApplication::widgetAt(e->globalPos())!=tabBar) {
            return;
        }
    }
    else {
        switch (styleSetting&Position_Mask) {
        case Bot:
            if (e->pos().y()<=(sideWidget->pos().y()+(sideWidget->height()-tabBar->height()))) {
                return;
            }
            break;
        case Top:
            if (e->pos().y()>(sideWidget->pos().y()+tabBar->height())) {
                return;
            }
            break;
        default:
            if (Qt::RightToLeft==QApplication::layoutDirection()) {
                if (e->pos().x()<=sideWidget->pos().x()) {
                    return;
                }
            } else if (e->pos().x()>=sideWidget->rect().right()) {
                return;
            }
        }
    }

    if (!menu) {
        menu = new QMenu(this);
        QAction *act=new QAction(i18n("Configure..."), this);
        connect(act, SIGNAL(triggered()), SIGNAL(configRequested()));
        menu->addAction(act);
    }
    menu->popup(e->globalPos());
}

void FancyTabWidget::setStyle(int s)
{
    if(s==styleSetting && tabBar) {
        return;
    }
    // Remove previous tab bar
    delete tabBar;
    tabBar = NULL;

    //   use_background_ = false;
    // Create new tab bar
    if (Tab==(s&Style_Mask) || (Small==(s&Style_Mask) && !(s&IconOnly))) {
        switch (s&Position_Mask) {
        default:
        case Side:
            makeTabBar(Qt::RightToLeft==QApplication::layoutDirection() ? QTabBar::RoundedEast : QTabBar::RoundedWest, !(s&IconOnly), true, Small==(s&Style_Mask));
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
