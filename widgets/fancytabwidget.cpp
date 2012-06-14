/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "settings.h"
#include "localize.h"
// #include "stylehelper.h"

// #include <QtGui/QColorDialog>
#include <QtGui/QHBoxLayout>
#include <QtGui/QMenu>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QSplitter>
#include <QtGui/QStackedLayout>
#include <QtGui/QStyleOptionTabV3>
#include <QtGui/QToolButton>
#include <QtGui/QToolTip>
#include <QtGui/QVBoxLayout>
#include <QtGui/QWindowsStyle>
#include <QtGui/QApplication>
#include <QtCore/QAnimationGroup>
#include <QtCore/QPropertyAnimation>
#include <QtCore/QSignalMapper>

static inline bool isGtkStyle()
{
    return QApplication::style()->inherits("QGtkStyle");
}

using namespace Core;
using namespace Internal;

const int FancyTabBar::m_rounding = 22;
const int FancyTabBar::m_textPadding = 4;

#if 0
static QPainterPath createPath(const QRect &rect, double radius)
{
    QPainterPath path;
    double diameter(radius*2);
    QRectF r(rect.x()+0.5, rect.y()+0.5, rect.width()-1, rect.height()-1);

    path.moveTo(r.x()+r.width(), r.y()+r.height()-radius);
    path.arcTo(r.x()+r.width()-diameter, r.y(), diameter, diameter, 0, 90);
    path.arcTo(r.x(), r.y(), diameter, diameter, 90, 90);
    path.arcTo(r.x(), r.y()+r.height()-diameter, diameter, diameter, 180, 90);
    path.arcTo(r.x()+r.width()-diameter, r.y()+r.height()-diameter, diameter, diameter, 270, 90);

    return path;
}

static void drawRoundedRect(QPainter *p, const QRect &r, double radius, const QColor &color)
{
    QPainterPath path(createPath(r, radius));
    QLinearGradient grad(QPoint(r.x(), r.y()), QPoint(r.x(), r.bottom()));
    QColor col(color);

    grad.setColorAt(0, col.lighter(120));
    grad.setColorAt(1.0, col.darker(120));

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->fillPath(path, grad);
    p->setPen(col);
    p->drawPath(path);
    p->restore();
}
#endif

static void drawIcon(const QIcon &icon, const QRect &r, QPainter *p, const QSize &iconSize)
{
    QPixmap px = icon.pixmap(iconSize);
    p->drawPixmap(r.x()+(r.width()-px.width())/2.0, r.y()+(r.height()-px.height())/2.0, px.width(), px.height(), px);
}

void FancyTabProxyStyle::drawControl(
    ControlElement element, const QStyleOption* option,
    QPainter* p, const QWidget* widget) const {

  const QStyleOptionTabV3* v_opt = qstyleoption_cast<const QStyleOptionTabV3*>(option);

  if (element != CE_TabBarTab || !v_opt) {
    QProxyStyle::drawControl(element, option, p, widget);
    return;
  }

  const QRect rect = v_opt->rect;
  const bool selected = v_opt->state & State_Selected;
  const bool vertical_tabs = v_opt->shape == QTabBar::RoundedWest;
  const QString text = v_opt->text;

  QTransform m;
  if (vertical_tabs) {
    m = QTransform::fromTranslate(rect.left(), rect.bottom());
    m.rotate(-90);
  } else {
    m = QTransform::fromTranslate(rect.left(), rect.top());
  }

  const QRect draw_rect(QPoint(0, 0), m.mapRect(rect).size());

  p->save();
  p->setTransform(m);

  QRect icon_rect(QPoint(8, 0), v_opt->iconSize);
  QRect text_rect(icon_rect.topRight() + QPoint(4, 0), draw_rect.size());
  text_rect.setRight(draw_rect.width());
  icon_rect.translate(0, (draw_rect.height() - icon_rect.height()) / 2);

    QStyleOptionViewItemV4 styleOpt;
    styleOpt.palette=option->palette;
    styleOpt.rect=draw_rect;
    if (QStyleOptionTab::Beginning==v_opt->position) {
        styleOpt.rect.adjust(0, 0, -1, 0);
    }
    styleOpt.state=option->state;
    styleOpt.state&=~(QStyle::State_Selected|QStyle::State_MouseOver);
    styleOpt.state|=QStyle::State_Selected|QStyle::State_Enabled;
    styleOpt.viewItemPosition = QStyleOptionViewItemV4::OnlyOne;
    styleOpt.showDecorationSelected=true;
    bool drawBgnd=true;

    if (!selected && drawBgnd) {
        const QString fader_key = "tab_" + text + "_fader";
        const QString animation_key = "tab_" + text + "_animation";

        const QString tab_hover = widget->property("tab_hover").toString();
        int fader = widget->property(fader_key.toUtf8().constData()).toInt();
        QPropertyAnimation* animation = widget->property(animation_key.toUtf8().constData()).value<QPropertyAnimation*>();

        if (!animation) {
            QWidget* mut_widget = const_cast<QWidget*>(widget);
            fader = 0;
            mut_widget->setProperty(fader_key.toUtf8().constData(), fader);
            animation = new QPropertyAnimation(mut_widget, fader_key.toUtf8(), mut_widget);
            connect(animation, SIGNAL(valueChanged(QVariant)), mut_widget, SLOT(update()));
            mut_widget->setProperty(animation_key.toUtf8().constData(), QVariant::fromValue(animation));
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

        if (fader<1 || (50!=fader && isGtkStyle())) {
            drawBgnd=false;
        } else {
            QColor col(styleOpt.palette.highlight().color());
            col.setAlpha(fader);
            styleOpt.palette.setColor(styleOpt.palette.currentColorGroup(), QPalette::Highlight, col);
        }
    }

    if (drawBgnd) {
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &styleOpt, p, 0);
    }
//     drawRoundedRect(p, draw_rect, 3, col);


//   QFont boldFont(p->font());
//   boldFont.setPointSizeF(Utils::StyleHelper::sidebarFontSize());
// //   boldFont.setBold(true);
//   p->setFont(boldFont);
//   p->setPen(selected ? QColor(255, 255, 255, 160) : QColor(0, 0, 0, 110));
  int textFlags = Qt::AlignTop | Qt::AlignVCenter;
//   p->drawText(text_rect, textFlags, text);
//   p->setPen(selected ? QColor(60, 60, 60) : Utils::StyleHelper::panelTextColor());
    p->setPen(selected ? QApplication::palette().highlightedText().color() : QApplication::palette().foreground().color());

  drawIcon(v_opt->icon, icon_rect, p, v_opt->iconSize);

  QString txt=text;
  txt.replace("&", "");
  txt=p->fontMetrics().elidedText(txt, Qt::ElideMiddle, text_rect.width());

  p->drawText(text_rect.translated(0, -1), textFlags, txt);

  p->restore();
}

void FancyTabProxyStyle::polish(QWidget* widget) {
  if (QString(widget->metaObject()->className()) == "QTabBar") {
    widget->setMouseTracking(true);
    widget->installEventFilter(this);
  }
  QProxyStyle::polish(widget);
}

void FancyTabProxyStyle::polish(QApplication* app) {
  QProxyStyle::polish(app);
}

void FancyTabProxyStyle::polish(QPalette& palette) {
  QProxyStyle::polish(palette);
}

bool FancyTabProxyStyle::eventFilter(QObject* o, QEvent* e) {
  QTabBar* bar = qobject_cast<QTabBar*>(o);
  if (bar && (e->type() == QEvent::MouseMove || e->type() == QEvent::Leave)) {
    QMouseEvent* event = static_cast<QMouseEvent*>(e);
    const QString old_hovered_tab = bar->property("tab_hover").toString();
    const QString hovered_tab = e->type() == QEvent::Leave ? QString() : bar->tabText(bar->tabAt(event->pos()));
    bar->setProperty("tab_hover", hovered_tab);

    if (old_hovered_tab != hovered_tab)
      bar->update();
  }

  return false;
}

FancyTab::FancyTab(QWidget* tabbar)
  : QWidget(tabbar), tabbar(tabbar), m_fader(0)
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
    m_fader = value;
    tabbar->update();
}

FancyTabBar::FancyTabBar(QWidget *parent, bool hasBorder, bool text, int iSize)
    : QWidget(parent)
    , m_hasBorder(hasBorder)
    , m_showText(text)
    , m_iconSize(iSize)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    setStyle(new QWindowsStyle);
    setMinimumWidth(28);
    setAttribute(Qt::WA_Hover, true);
    setFocusPolicy(Qt::NoFocus);
    setMouseTracking(true); // Needed for hover events
    m_triggerTimer.setSingleShot(true);

    QVBoxLayout* layout = new QVBoxLayout;
    layout->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));
    layout->setSpacing(0);
    layout->setContentsMargins(0, 0, 0, 0);
    setLayout(layout);

    // We use a zerotimer to keep the sidebar responsive
    connect(&m_triggerTimer, SIGNAL(timeout()), this, SLOT(emitCurrentIndex()));
}

FancyTabBar::~FancyTabBar()
{
    delete style();
}

QSize FancyTab::sizeHint() const {
//   QFont boldFont(font());
//   boldFont.setPointSizeF(Utils::StyleHelper::sidebarFontSize());
//   boldFont.setBold(true);
    int iconSize=static_cast<FancyTabBar *>(parent())->iconSize();
    const int spacing = 8;
    if (static_cast<FancyTabBar *>(parent())->showText()) {
        QFontMetrics fm(font());
        int textWidth = fm.width(text);
        int width = qMax(32, qMin(96, textWidth)) + spacing + 2;
        QSize ret(width, iconSize + spacing + fm.height());
        return ret;
    } else {
        return QSize(iconSize + spacing, iconSize + spacing);
    }
}

QSize FancyTabBar::tabSizeHint() const
{
//   QFont boldFont(font());
//   boldFont.setPointSizeF(Utils::StyleHelper::sidebarFontSize());
//   boldFont.setBold(true);
    const int spacing = 8;
    if (m_showText) {
        QFontMetrics fm(font());
        int maxTw=0;
        foreach (FancyTab *tab, m_tabs) {
            maxTw=qMax(maxTw, tab->sizeHint().width());
        }
        return QSize(qMax(32 + spacing + 2, maxTw), m_iconSize + spacing + fm.height());
    } else {
        return QSize(m_iconSize + spacing, m_iconSize + spacing);
    }
}

void FancyTabBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter p(this);
    bool gtkStyle=isGtkStyle();

    for (int i = 0; i < count(); ++i)
        if (i != currentIndex())
            paintTab(&p, i, gtkStyle);

    // paint active tab last, since it overlaps the neighbors
    if (currentIndex() != -1)
        paintTab(&p, currentIndex(), gtkStyle);
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
    return QSize(sh.width(), sh.height() * m_tabs.count());
}

QSize FancyTabBar::minimumSizeHint() const
{
    QSize sh = tabSizeHint();
    return QSize(sh.width(), sh.height() * m_tabs.count());
}

QRect FancyTabBar::tabRect(int index) const
{
    return m_tabs[index]->geometry();
}

QString FancyTabBar::tabToolTip(int index) const {
  return m_tabs[index]->toolTip();
}

void FancyTabBar::setTabToolTip(int index, const QString& toolTip) {
  m_tabs[index]->setToolTip(toolTip);
}

// This keeps the sidebar responsive since
// we get a repaint before loading the
// mode itself
void FancyTabBar::emitCurrentIndex()
{
    emit currentChanged(m_currentIndex);
}

void FancyTabBar::mousePressEvent(QMouseEvent *e)
{
    if (Qt::LeftButton!=e->button())
        return;
    e->accept();
    for (int index = 0; index < m_tabs.count(); ++index) {
        if (tabRect(index).contains(e->pos())) {
            m_currentIndex = index;
            update();
            m_triggerTimer.start(0);
            break;
        }
    }
}

void FancyTabBar::addTab(const QIcon& icon, const QString& label) {
  FancyTab *tab = new FancyTab(this);
  tab->icon = icon;
  tab->text = label;
  m_tabs.append(tab);
  if (!m_showText) {
      tab->setToolTip(label);
  }
  qobject_cast<QVBoxLayout*>(layout())->insertWidget(layout()->count()-1, tab);
}

void FancyTabBar::addSpacer(int size) {
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
    bool selected = (tabIndex == m_currentIndex);

    if (m_hasBorder) {
        rect.adjust(2, 0, -2, 0);
    }
    QStyleOptionViewItemV4 styleOpt;
    styleOpt.initFrom(this);
    styleOpt.state&=~(QStyle::State_Selected|QStyle::State_MouseOver);
    styleOpt.state|=QStyle::State_Selected|QStyle::State_Enabled;
    styleOpt.rect=rect;
    styleOpt.viewItemPosition = QStyleOptionViewItemV4::OnlyOne;
    styleOpt.showDecorationSelected=true;
    bool drawBgnd=true;

    if (!selected && drawBgnd) {
        int fader=int(m_tabs[tabIndex]->fader());
        if (fader<1 || (gtkStyle && 50!=fader)) {
            drawBgnd=false;
        } else  {
            QColor col(styleOpt.palette.highlight().color());
            col.setAlpha(fader);
            styleOpt.palette.setColor(styleOpt.palette.currentColorGroup(), QPalette::Highlight, col);
        }
    }
    if (drawBgnd) {
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &styleOpt, painter, 0);
    }
//    drawRoundedRect(painter, rect, 3, col);


    if (m_showText) {
        QString tabText(painter->fontMetrics().elidedText(this->tabText(tabIndex), Qt::ElideMiddle, width()));
        QRect tabTextRect(tabRect(tabIndex));
        QRect tabIconRect(tabTextRect);
        tabIconRect.adjust(+4, +4, -4, -4);
        tabTextRect.translate(0, -2);
//         QFont boldFont(painter->font());
//         boldFont.setPointSizeF(Utils::StyleHelper::sidebarFontSize());
//         boldFont.setBold(true);
//         painter->setFont(boldFont);
//         painter->setPen(selected ? QColor(255, 255, 255, 160) : QColor(0, 0, 0, 110));
        painter->setPen(selected ? palette().highlightedText().color() : palette().foreground().color());
        int textFlags = Qt::AlignCenter | Qt::AlignBottom;
        painter->drawText(tabTextRect, textFlags, tabText);

        const int textHeight = painter->fontMetrics().height();
        tabIconRect.adjust(0, 4, 0, -textHeight);
//         Utils::StyleHelper::drawIconWithShadow(tabIcon(tabIndex), tabIconRect, painter, QIcon::Normal);
        drawIcon(tabIcon(tabIndex), tabIconRect, painter, QSize(m_iconSize, m_iconSize));
    } else {
        drawIcon(tabIcon(tabIndex), rect, painter, QSize(m_iconSize, m_iconSize));
    }


//     painter->translate(0, -1);
//     painter->drawText(tabTextRect, textFlags, tabText);
    painter->restore();
}

void FancyTabBar::setCurrentIndex(int index) {
    m_currentIndex = index;
    update();
    emit currentChanged(m_currentIndex);
}


// //////
// // FancyColorButton
// //////
//
// class FancyColorButton : public QWidget
// {
// public:
//     FancyColorButton(QWidget *parent)
//       : m_parent(parent)
//     {
//         setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
//     }
//
//     void mousePressEvent(QMouseEvent *ev)
//     {
//         if (ev->modifiers() & Qt::ShiftModifier)
//             Utils::StyleHelper::setBaseColor(QColorDialog::getColor(Utils::StyleHelper::requestedBaseColor(), m_parent));
//     }
// private:
//     QWidget *m_parent;
// };

//////
// FancyTabWidget
//////

FancyTabWidget::FancyTabWidget(QWidget* parent, bool allowContext, bool drawBorder)
  : QWidget(parent),
    mode_(Mode_None),
    tab_bar_(NULL),
    stack_(new QStackedLayout),
    side_widget_(new QWidget),
    side_layout_(new QVBoxLayout),
    top_layout_(new QVBoxLayout),
//     use_background_(false),
    menu_(NULL),
    proxy_style_(new FancyTabProxyStyle),
    allowContext_(allowContext),
    drawBorder_(drawBorder)
{
  side_layout_->setSpacing(0);
  side_layout_->setMargin(0);
  side_layout_->addSpacerItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));

  side_widget_->setLayout(side_layout_);
  side_widget_->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

  top_layout_->setMargin(0);
  top_layout_->setSpacing(0);
  top_layout_->addLayout(stack_);

  QHBoxLayout* main_layout = new QHBoxLayout;
  main_layout->setMargin(0);
  main_layout->setSpacing(1);
  main_layout->addWidget(side_widget_);
  main_layout->addLayout(top_layout_);
  setLayout(main_layout);
}

void FancyTabWidget::AddTab(QWidget* tab, const QIcon& icon, const QString& label, bool enabled) {
  stack_->addWidget(tab);
  items_ << Item(icon, label, enabled);
  setMinimumWidth(128);
}

void FancyTabWidget::AddSpacer(int size) {
  items_ << Item(size);
}

void FancyTabWidget::SetBackgroundPixmap(const QPixmap& pixmap) {
  background_pixmap_ = pixmap;
  update();
}

static void drawFadedLine(QPainter *p, const QRect &r, const QColor &col)
{
    QPoint start(r.x(), r.y());
    QPoint end(r.x(), r.y()+r.height()-1);
    QLinearGradient grad(start, end);
    QColor c(col);
    c.setAlphaF(0.45);
    QColor fade(c);
    const int fadeSize=32;
    double fadePos=1.0-((r.height()-(fadeSize*2))/(r.height()*1.0));

    fade.setAlphaF(0.0);
    if(fadePos>=0 && fadePos<=1.0) {
        grad.setColorAt(0, fade);
        grad.setColorAt(fadePos, c);
    } else {
        grad.setColorAt(0, c);
    }
    if(fadePos>=0 && fadePos<=1.0) {
        grad.setColorAt(1.0-fadePos, c);
        grad.setColorAt(1, fade);
    } else {
        grad.setColorAt(1, c);
    }
    p->save();
    p->setPen(QPen(QBrush(grad), 1));
    p->drawLine(start, end);
    p->restore();
}

void FancyTabWidget::paintEvent(QPaintEvent*e) {
    QWidget::paintEvent(e);
    if (!drawBorder_) {
        return;
    }

    QPainter painter(this);
    QRect rect = side_widget_->rect().adjusted(0, 0, 1, 0);
    rect = style()->visualRect(layoutDirection(), geometry(), rect);
    drawFadedLine(&painter, rect, palette().foreground().color());
    drawFadedLine(&painter, rect.adjusted(rect.width()-2, 0, 0, 0), palette().foreground().color());
//     painter.setPen(QApplication::palette().mid().color());
//     painter.drawLine(rect.topRight(), rect.bottomRight());
//   if (!use_background_)
//     return;
//
//   QPainter painter(this);
//
//   QRect rect = side_widget_->rect().adjusted(0, 0, 1, 0);
//   rect = style()->visualRect(layoutDirection(), geometry(), rect);
//   Utils::StyleHelper::verticalGradient(&painter, rect, rect);
//
//   if (!background_pixmap_.isNull()) {
//     QRect pixmap_rect(background_pixmap_.rect());
//     pixmap_rect.moveTo(rect.topLeft());
//
//     while (pixmap_rect.top() < rect.bottom()) {
//       QRect source_rect(pixmap_rect.intersected(rect));
//       source_rect.moveTo(0, 0);
//       painter.drawPixmap(pixmap_rect.topLeft(), background_pixmap_, source_rect);
//       pixmap_rect.moveTop(pixmap_rect.bottom() - 10);
//     }
//   }
//
//   painter.setPen(Utils::StyleHelper::borderColor());
//   painter.drawLine(rect.topRight(), rect.bottomRight());
//
//   QColor light = Utils::StyleHelper::sidebarHighlight();
//   painter.setPen(light);
//   painter.drawLine(rect.bottomLeft(), rect.bottomRight());
}

int FancyTabWidget::current_index() const {
  return stack_->currentIndex();
}

QWidget * FancyTabWidget::currentWidget() const {
  return stack_->currentWidget();
}

QWidget * FancyTabWidget::widget(int index) const {
  return stack_->widget(index);
}

int FancyTabWidget::count() const {
  return stack_->count();
}

int FancyTabWidget::visibleCount() const {
    int c=0;

    foreach (const Item &i, items_) {
        if (i.enabled_) {
            c++;
        }
    }
    return c;
}

void FancyTabWidget::SetCurrentIndex(int idx) {
  if (!isEnabled(idx)) {
    return;
  }
  int index=IndexToTab(idx);
  if (FancyTabBar* bar = qobject_cast<FancyTabBar*>(tab_bar_)) {
    bar->setCurrentIndex(index);
  } else if (QTabBar* bar = qobject_cast<QTabBar*>(tab_bar_)) {
    bar->setCurrentIndex(index);
  } else {
    stack_->setCurrentIndex(idx); // ?? IS this *ever* called???
  }
}

void FancyTabWidget::ShowWidget(int index) {
  int idx=TabToIndex(index);
  stack_->setCurrentIndex(idx);
  emit CurrentChanged(idx);
}

void FancyTabWidget::AddBottomWidget(QWidget* widget) {
  top_layout_->addWidget(widget);
}

void FancyTabWidget::SetMode(Mode mode) {
  if(mode==mode_) {
      return;
  }
  // Remove previous tab bar
  delete tab_bar_;
  tab_bar_ = NULL;

//   use_background_ = false;

  // Create new tab bar
  switch (mode) {
    case Mode_None:
    default:
//       qDebug() << "Unknown fancy tab mode" << mode;
      // fallthrough

    case Mode_IconOnlySmallSidebar:
    case Mode_IconOnlyLargeSidebar:
    case Mode_LargeSidebar: {
      FancyTabBar* bar = new FancyTabBar(this, drawBorder_, Mode_LargeSidebar==mode, Mode_IconOnlySmallSidebar==mode ? 16 : 32);
      side_layout_->insertWidget(0, bar);
      tab_bar_ = bar;

      int index=0;
      QList<Item>::Iterator it=items_.begin();
      QList<Item>::Iterator end=items_.end();
      for (; it!=end; ++it) {
        if (!(*it).enabled_) {
          (*it).index_=-1;
          continue;
        }
        if ((*it).type_ == Item::Type_Spacer)
          bar->addSpacer((*it).spacer_size_);
        else
          bar->addTab((*it).tab_icon_, (*it).tab_label_);
        (*it).index_=index++;
      }

      bar->setCurrentIndex(IndexToTab(stack_->currentIndex()));
      connect(bar, SIGNAL(currentChanged(int)), SLOT(ShowWidget(int)));

//       use_background_ = true;

      break;
    }

//    case Mode_Tabs:
//      MakeTabBar(QTabBar::RoundedNorth, true, false, false);
//      break;

    case Mode_IconOnlyTopTabs:
      MakeTabBar(QTabBar::RoundedNorth, false, true, false);
      break;

    case Mode_TopTabs:
      MakeTabBar(QTabBar::RoundedNorth, true, true, false);
      break;

    case Mode_IconOnlyBotTabs:
      MakeTabBar(QTabBar::RoundedSouth, false, true, false);
      break;

    case Mode_BotTabs:
      MakeTabBar(QTabBar::RoundedSouth, true, true, false);
      break;

    case Mode_SmallSidebar:
      MakeTabBar(QTabBar::RoundedWest, true, true, true);
//       use_background_ = true;
      break;

//    case Mode_PlainSidebar:
    case Mode_SideTabs:
      MakeTabBar(Qt::RightToLeft==QApplication::layoutDirection() ? QTabBar::RoundedEast : QTabBar::RoundedWest, true, true, false);
      break;

    case Mode_IconOnlySideTabs:
      MakeTabBar(Qt::RightToLeft==QApplication::layoutDirection() ? QTabBar::RoundedEast : QTabBar::RoundedWest, false, true, false);
      break;
  }

  tab_bar_->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

  mode_ = mode;
  emit ModeChanged(mode);
  update();
}

void FancyTabWidget::ToggleTab(int tab, bool show) {
    if (tab>=0 && tab<items_.count()) {
        items_[tab].enabled_=show;
        Mode m=mode_;
        mode_=Mode_None;
        SetMode(m);
        emit TabToggled(tab);
    }
}

void FancyTabWidget::ToggleTab() {
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        ToggleTab(act->data().toInt(), act->isChecked());
    }
}

void FancyTabWidget::contextMenuEvent(QContextMenuEvent* e) {
  if (!allowContext_) {
      return;
  }

  // Check we are over tab space...
  if (Mode_IconOnlyTopTabs!=mode_ && Mode_TopTabs!=mode_ && Mode_IconOnlyBotTabs!=mode_ && Mode_BotTabs!=mode_){
      if (Qt::RightToLeft==QApplication::layoutDirection()) {
          if (e->pos().x()<=side_widget_->pos().x()) {
            return;
          }
      } else if (e->pos().x()>=side_widget_->rect().right()) {
          return;
      }
  } else if (QApplication::widgetAt(e->globalPos())!=tab_bar_) {
      return;
  }
  if (!menu_) {
    menu_ = new QMenu(this);

    int idx=0;
    foreach (const Item& item, items_) {
        if (Item::Type_Tab==item.type_) {
            QAction* action = menu_->addAction(item.tab_icon_, item.tab_label_);
            action->setCheckable(true);
            action->setChecked(item.enabled_);
            action->setData(idx);
            connect(action, SIGNAL(triggered()), this, SLOT(ToggleTab()));
        }
        idx++;
    }
    menu_->addSeparator();

    QActionGroup* group = new QActionGroup(this);
    QMenu *modeMenu=new QMenu(this);
    QAction *modeAct;
    QAction *iconOnlyAct;
    QAction *autoHideAct;
    iconOnlyAct=new QAction(i18n("Icons Only"), this);
    modeAct=new QAction(i18n("Style"), this);
    autoHideAct=new QAction(i18n("Auto Hide"), this);
    AddMenuItem(group, i18n("Large Sidebar"), Mode_LargeSidebar, Mode_IconOnlyLargeSidebar);
    AddMenuItem(group, i18n("Small Sidebar"), Mode_SmallSidebar, Mode_IconOnlySmallSidebar);
    AddMenuItem(group, i18n("Tabs On Side"), Mode_SideTabs, Mode_IconOnlySideTabs);
    AddMenuItem(group, i18n("Tabs On Top"), Mode_TopTabs, Mode_IconOnlyTopTabs);
    AddMenuItem(group, i18n("Tabs On Bottom"), Mode_BotTabs, Mode_IconOnlyBotTabs);
    modeMenu->addActions(group->actions());
    iconOnlyAct->setCheckable(true);
    iconOnlyAct->setChecked(Mode_IconOnlyLargeSidebar==mode_ || Mode_IconOnlySmallSidebar==mode_ ||
                            Mode_IconOnlySideTabs==mode_ || Mode_IconOnlyTopTabs==mode_ ||
                            Mode_IconOnlyBotTabs==mode_);
    iconOnlyAct->setData(0);
    connect(iconOnlyAct, SIGNAL(triggered()), this, SLOT(SetMode()));
    modeMenu->addAction(iconOnlyAct);
    modeAct->setMenu(modeMenu);
    modeAct->setData(-1);
    menu_->addAction(modeAct);
    autoHideAct->setCheckable(true);
    autoHideAct->setChecked(Settings::self()->splitterAutoHide());
    autoHideAct->setData(-1);
    connect(autoHideAct, SIGNAL(triggered()), this, SLOT(SetAutoHide()));
    menu_->addAction(autoHideAct);
  }

  foreach (QAction *act, menu_->actions()) {
      if (act->data().toInt()==stack_->currentIndex()) {
          act->setEnabled(false);
      } else {
          act->setEnabled(true);
      }
  }
  menu_->popup(e->globalPos());
}

void FancyTabWidget::AddMenuItem(QActionGroup* group, const QString& text, Mode mode, Mode iconMode) {
  QAction* action = group->addAction(text);
  action->setCheckable(true);
  action->setData(mode);
  connect(action, SIGNAL(triggered()), this, SLOT(SetMode()));

  if (mode == mode_ || iconMode==mode_) {
    action->setChecked(true);
  }
}

void FancyTabWidget::SetAutoHide()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (act) {
        emit AutoHideChanged(act->isChecked());
    }
}

void FancyTabWidget::SetMode()
{
    QAction *act=qobject_cast<QAction *>(sender());

    if (act) {
        int data=act->data().toInt();

        if (0==data) {
            switch (mode_) {
            case Mode_LargeSidebar:         SetMode(Mode_IconOnlyLargeSidebar); break;
            case Mode_SmallSidebar:         SetMode(Mode_IconOnlySmallSidebar); break;
            case Mode_SideTabs:             SetMode(Mode_IconOnlySideTabs); break;
            case Mode_TopTabs:              SetMode(Mode_IconOnlyTopTabs); break;
            case Mode_IconOnlyTopTabs:      SetMode(Mode_TopTabs); break;
            case Mode_BotTabs:              SetMode(Mode_IconOnlyBotTabs); break;
            case Mode_IconOnlyBotTabs:      SetMode(Mode_BotTabs); break;
            case Mode_IconOnlyLargeSidebar: SetMode(Mode_LargeSidebar); break;
            case Mode_IconOnlySmallSidebar: SetMode(Mode_SmallSidebar); break;
            case Mode_IconOnlySideTabs:     SetMode(Mode_SideTabs); break;
            default: break;
            }
        } else {
            bool iconOnly=Mode_IconOnlyLargeSidebar==mode_ || Mode_IconOnlySmallSidebar==mode_ ||
                          Mode_IconOnlySideTabs==mode_ || Mode_IconOnlyTopTabs==mode_ ||
                          Mode_IconOnlyBotTabs==mode_;
            switch (data) {
            case Mode_LargeSidebar: SetMode(iconOnly ? Mode_IconOnlyLargeSidebar : Mode_LargeSidebar); break;
            case Mode_SmallSidebar: SetMode(iconOnly ? Mode_IconOnlySmallSidebar : Mode_SmallSidebar); break;
            case Mode_SideTabs:     SetMode(iconOnly ? Mode_IconOnlySideTabs : Mode_SideTabs); break;
            case Mode_TopTabs:      SetMode(iconOnly ? Mode_IconOnlyTopTabs : Mode_TopTabs); break;
            case Mode_BotTabs:      SetMode(iconOnly ? Mode_IconOnlyBotTabs : Mode_BotTabs); break;
            default: break;
            }
        }
    }
}

void FancyTabWidget::MakeTabBar(QTabBar::Shape shape, bool text, bool icons,
                                bool fancy) {
  QTabBar* bar = new QTabBar(this);
  bar->setShape(shape);
  bar->setDocumentMode(true);
  bar->setUsesScrollButtons(true);

  if (shape == QTabBar::RoundedWest) {
    bar->setIconSize(QSize(16, 16));
  }

  if (fancy) {
    bar->setStyle(proxy_style_.data());
  }

  if (shape == QTabBar::RoundedNorth)
    top_layout_->insertWidget(0, bar);
  else if (shape == QTabBar::RoundedSouth)
      top_layout_->insertWidget(1, bar);
  else
    side_layout_->insertWidget(0, bar);

  int index=0;
  QList<Item>::Iterator it=items_.begin();
  QList<Item>::Iterator end=items_.end();
  for (; it!=end; ++it) {
    if ((*it).type_ != Item::Type_Tab || !(*it).enabled_) {
      (*it).index_=-1;
      continue;
    }

    QString label = (*it).tab_label_;
    if (shape == QTabBar::RoundedWest) {
      label = QFontMetrics(font()).elidedText(label, Qt::ElideMiddle, 100);
    }

    int tab_id = -1;
    if (icons && text)
      tab_id = bar->addTab((*it).tab_icon_, label);
    else if (icons)
      tab_id = bar->addTab((*it).tab_icon_, QString());
    else if (text)
      tab_id = bar->addTab(label);

    bar->setTabToolTip(tab_id, (*it).tab_label_);
    (*it).index_=index++;
  }

  bar->setCurrentIndex(IndexToTab(stack_->currentIndex()));
  connect(bar, SIGNAL(currentChanged(int)), SLOT(ShowWidget(int)));
  tab_bar_ = bar;
}

int FancyTabWidget::TabToIndex(int tab) const
{
    for (int i=0; i<items_.count(); ++i) {
        if (items_[i].index_==tab) {
            return i;
        }
    }

    return 0;
}
