/*
 * Cantata
 *
 * Copyright (c) 2011-2020 Craig Drummond <craig.p.drummond@gmail.com>
 *
 * ----
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "configdialog.h"
#include "utils.h"
#ifdef __APPLE__
#include <QBoxLayout>
#include <QToolBar>
#include <QToolButton>
#include <QDialogButtonBox>
#include <QBoxLayout>
#include <QPushButton>
#include <QButtonGroup>
#include <QStackedWidget>
#include <QStylePainter>
#include <QPainter>
#include <QColor>
#include <QRect>
#include <QLinearGradient>
#include <QStyleOptionToolButton>
#include <QPointF>
#include <QKeyEvent>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QAbstractItemView>
#include <QTimer>
#include <QPropertyAnimation>
#include <QSysInfo>
#include <QDesktopWidget>
#include "icon.h"
#include "osxstyle.h"
#include "windowmanager.h"
#else
#include "pagewidget.h"
#endif

#ifdef __APPLE__
//#define ANIMATE_RESIZE
static void drawFadedLine(QPainter *p, const QRect &r, const QColor &col, bool horiz, double fadeSize)
{
    QPointF start(r.x()+0.5, r.y()+0.5);
    QPointF end(r.x()+(horiz ? r.width()-1 : 0)+0.5,
                r.y()+(horiz ? 0 : r.height()-1)+0.5);
    QLinearGradient grad(start, end);
    QColor fade(col);

    fade.setAlphaF(0.0);
    grad.setColorAt(0, fade);
    grad.setColorAt(fadeSize, col);
    grad.setColorAt(1.0-fadeSize, col);
    grad.setColorAt(1, fade);
    p->setPen(QPen(QBrush(grad), 1));
    p->drawLine(start, end);
}

class ConfigDialogToolButton : public QToolButton
{
public:
    ConfigDialogToolButton(QWidget *p)
        : QToolButton(p)
    {
        setStyleSheet("QToolButton {border: 0}");
        setMinimumWidth(56);
    }

    void paintEvent(QPaintEvent *e)
    {
        if (isChecked()) {
            QPainter p(this);
            QColor col(Qt::black);
            QRect r(rect());

            if (QSysInfo::MacintoshVersion > QSysInfo::MV_10_9) {
                col.setAlphaF(0.1);
                p.setClipRect(r);
                p.setRenderHint(QPainter::Antialiasing, true);
                p.fillPath(Utils::buildPath(r.adjusted(0, 0, 0, 32), 4), col);
            } else {
                QLinearGradient bgndGrad(r.topLeft(), r.bottomLeft());
                col.setAlphaF(0.01);
                bgndGrad.setColorAt(0, col);
                bgndGrad.setColorAt(1, col);
                col.setAlphaF(0.1);
                bgndGrad.setColorAt(0.25, col);
                bgndGrad.setColorAt(0.75, col);
                p.fillRect(r, bgndGrad);
                p.setRenderHint(QPainter::Antialiasing, true);
                col.setAlphaF(0.25);
                drawFadedLine(&p, QRect(r.x(), r.y(), r.x(), r.y()+r.height()-1), col, false, 0.25);
                drawFadedLine(&p, QRect(r.x()+r.width()-1, r.y(), r.x()+1, r.y()+r.height()-1), col, false, 0.25);
            }
        }
        QStylePainter p(this);
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        if (isDown()) {
            opt.state&=QStyle::State_Sunken;
        }
        p.drawComplexControl(QStyle::CC_ToolButton, opt);
    }
};

static const int constMinPad=16;
#endif

ConfigDialog::ConfigDialog(QWidget *parent, const QString &name, const QSize &defSize, bool instantApply)
    #ifdef __APPLE__
    : QMainWindow(parent)
    , shown(false)
    , resizeAnim(0)
    #else
    : Dialog(parent, name, defSize)
    #endif
{
    instantApply=false; // TODO!!!!

    #ifdef __APPLE__
    Q_UNUSED(defSize)
    Q_UNUSED(name)

    setUnifiedTitleAndToolBarOnMac(true);
    toolBar=addToolBar("ToolBar");
    toolBar->setMovable(false);
    toolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    toolBar->setStyleSheet("QToolBar { spacing:0px} ");
    QWidget *left=new QWidget(toolBar);
    left->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    toolBar->addWidget(left);
    QWidget *right=new QWidget(toolBar);
    right->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    rightSpacer=toolBar->addWidget(right);
    left->setMinimumWidth(constMinPad);
    right->setMinimumWidth(constMinPad);
    QWidget *mw=new QWidget(this);
    QBoxLayout *lay=new QBoxLayout(QBoxLayout::TopToBottom, mw);
    stack=new QStackedWidget(mw);
    group=new QButtonGroup(toolBar);
    lay->addWidget(stack);
    if (instantApply) {
        buttonBox=0;
        setWindowFlags(Qt::Window|Qt::WindowTitleHint|Qt::CustomizeWindowHint|Qt::WindowMaximizeButtonHint|Qt::WindowCloseButtonHint);
    } else {
        buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|QDialogButtonBox::Apply, Qt::Horizontal, mw);
        lay->addWidget(buttonBox);
        connect(buttonBox, SIGNAL(clicked(QAbstractButton *)), this, SLOT(macButtonPressed(QAbstractButton *)));
        buttonBox->setStyle(Dialog::buttonProxyStyle());
        // Hide window buttons if not instany apply - dont want user just closing dialog
        setWindowFlags(Qt::Window|Qt::WindowTitleHint|Qt::CustomizeWindowHint|Qt::WindowMaximizeButtonHint);
    }

    group->setExclusive(true);
    setCentralWidget(mw);
    WindowManager *wm=new WindowManager(toolBar);
    wm->initialize(WindowManager::WM_DRAG_MENU_AND_TOOLBAR);
    wm->registerWidgetAndChildren(toolBar);
    #else

    bool limitedHeight=Utils::limitedHeight(this);
    pageWidget = new PageWidget(this, limitedHeight, !limitedHeight);
    setMainWidget(pageWidget);
    if (instantApply) {
        // TODO: What???
    } else {
        setButtons(Dialog::Ok|Dialog::Cancel|Dialog::Apply);
    }
    #endif
}

ConfigDialog::~ConfigDialog()
{
    #ifdef __APPLE__
    OSXStyle::self()->removeWindow(this);
    #endif
}

void ConfigDialog::addPage(const QString &id, QWidget *widget, const QString &name, const QIcon &icon, const QString &header)
{
    #ifdef __APPLE__

    Page newPage;
    newPage.item=new ConfigDialogToolButton(this);
    newPage.item->setText(name);
    newPage.item->setIcon(icon);
    newPage.item->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    newPage.item->setCheckable(true);
    newPage.item->setChecked(false);
    newPage.item->setProperty("id", id);
    newPage.item->ensurePolished();
    toolBar->insertWidget(rightSpacer, newPage.item);
    newPage.widget=widget;
    newPage.index=stack->count();
    stack->addWidget(widget);
    group->addButton(newPage.item);
    pages.insert(id, newPage);
    connect(newPage.item, SIGNAL(toggled(bool)), this, SLOT(activatePage()));
    int sz=0;
    QMap<QString, Page>::const_iterator it=pages.begin();
    QMap<QString, Page>::const_iterator end=pages.end();
    for (; it!=end; ++it) {
        sz+=it.value().item->sizeHint().width()+4;
    }
    setMinimumWidth(sz+(2*constMinPad)+48);
    #else

    pages.insert(id, pageWidget->addPage(widget, name, icon, header));

    #endif
}

bool ConfigDialog::setCurrentPage(const QString &id)
{
    if (!pages.contains(id)) {
        return false;
    }

    #ifdef __APPLE__
    QMap<QString, Page>::const_iterator it=pages.find(id);
    if (it!=pages.end()) {
        stack->setCurrentIndex(it.value().index);
        setCaption(it.value().item->text());
        it.value().item->setChecked(true);
    }
    #ifdef ANIMATE_RESIZE
    if (isVisible()) {
        it.value().widget->ensurePolished();
        ensurePolished();
    }
    int newH=buttonBox
                ? (it.value().widget->sizeHint().height()+toolBar->height()+buttonBox->height()+layout()->spacing()+(2*layout()->margin()))
                : (it.value().widget->sizeHint().height()+toolBar->height()+(2*layout()->margin()));

    if (isVisible()) {
        if (newH!=height()) {
            if (!resizeAnim) {
                resizeAnim=new QPropertyAnimation(this, "h");
                resizeAnim->setDuration(250);
            }
            resizeAnim->setStartValue(height());
            resizeAnim->setEndValue(newH);
            resizeAnim->start();
        }
    } else {
        setFixedHeight(newH);
        resize(width(), newH);
        setMaximumHeight(QWIDGETSIZE_MAX);
    }
    #endif
    #else
    pageWidget->setCurrentPage(pages[id]);
    #endif
    return true;
}

#ifdef __APPLE__
void ConfigDialog::setH(int h)
{
    if (resizeAnim && h==resizeAnim->endValue().toInt()) {
        setMaximumHeight(QWIDGETSIZE_MAX);
        resize(width(), h);
    } else {
        setFixedHeight(h);
    }
}
#endif

QWidget * ConfigDialog::getPage(const QString &id) const
{
    if (!pages.contains(id)) {
        return nullptr;
    }

    #ifdef __APPLE__
    return pages[id].widget;
    #else
    return pages[id]->widget();
    #endif
}

void ConfigDialog::slotButtonClicked(int button)
{
    switch (button) {
    case Dialog::Ok:
    case Dialog::Apply:
        save();
        break;
    case Dialog::Cancel:
        cancel();
        reject();
        #ifndef __APPLE__
        // Need to call this - if not, when dialog is closed by window X control, it is not deleted!!!!
        Dialog::slotButtonClicked(button);
        #endif
        break;
    default:
        break;
    }

    if (Dialog::Ok==button) {
        accept();
    }

    #ifndef __APPLE__
    Dialog::slotButtonClicked(button);
    #endif
}

void ConfigDialog::activatePage()
{
    #ifdef __APPLE__
    QToolButton *item=qobject_cast<QToolButton *>(sender());
    if (item && item->isChecked()) {
        setCurrentPage(item->property("id").toString());
    }
    #endif
}

#ifdef __APPLE__
void ConfigDialog::accept()
{
    close();
}

void ConfigDialog::reject()
{
    close();
}

void ConfigDialog::keyPressEvent(QKeyEvent *e)
{
    // Only act on Esc if we are instant-apply (in which case there will be no button box)...
    if (!buttonBox && Qt::Key_Escape==e->key() && Qt::NoModifier==e->modifiers()) {
        close();
    }
}

void ConfigDialog::hideEvent(QHideEvent *e)
{
    OSXStyle::self()->removeWindow(this);
    QMainWindow::hideEvent(e);
}

void ConfigDialog::closeEvent(QCloseEvent *e)
{
    OSXStyle::self()->removeWindow(this);
    QMainWindow::closeEvent(e);
}

static bool isFocusable(QWidget *w)
{
    if (/*qobject_cast<QAbstractButton *>(w) || qobject_cast<QComboBox *>(w) ||*/ qobject_cast<QLineEdit *>(w) ||
        qobject_cast<QTextEdit *>(w) || qobject_cast<QAbstractItemView *>(w)) {
        if (w->isVisible() && w->isEnabled()) {
            return true;
        }
    }
    return false;
}

static QWidget *firstFocusableWidget(QWidget *w)
{
    if (!w) {
        return 0;
    }
    if (isFocusable(w)) {
        return w;
    }

    QObjectList children=w->children();
    // Try each child first...
    for (QObject *c: children) {
        if (qobject_cast<QWidget *>(c)) {
            QWidget *cw=static_cast<QWidget *>(c);
            if (isFocusable(cw)) {
                return cw;
            }
        }
    }
    // Now try grandchildren...
    for (QObject *c: children) {
        if (qobject_cast<QWidget *>(c)) {
            QWidget *f=firstFocusableWidget(static_cast<QWidget *>(c));
            if (f) {
                return f;
            }
        }
    }
    return 0;
}

void ConfigDialog::showEvent(QShowEvent *e)
{
    if (!shown) {
        QTimer::singleShot(0, this, SLOT(setFocus()));
        shown=true;
        ensurePolished();

        QDesktopWidget *dw=QApplication::desktop();
        if (dw) {
            move(QPoint((dw->availableGeometry(this).size().width()-width())/2, 86));
        }
    }
    OSXStyle::self()->addWindow(this);
    QMainWindow::showEvent(e);
    if (parentWidget()) {
        move(pos().x(), parentWidget()->pos().y()+16);
    }
}
#endif

void ConfigDialog::setFocus()
{
    #ifdef __APPLE__
    // When the pref dialog is shown, no widget has focus - and there is a small (4px x 4px?) upper left
    // corner of a focus highlight drawn in the upper left of the dialog.
    // To work-around this, after the dialog is shown, look for a widget that can be set to have focus...
    QWidget *w=firstFocusableWidget(stack->currentWidget());
    if (w) {
        w->setFocus();
    }
    #endif
}

void ConfigDialog::macButtonPressed(QAbstractButton *b)
{
    #ifdef __APPLE__
    if (!buttonBox) {
        return;
    }
    if (b==buttonBox->button(QDialogButtonBox::Ok)) {
        slotButtonClicked(Dialog::Ok);
    } else if (b==buttonBox->button(QDialogButtonBox::Apply)) {
        slotButtonClicked(Dialog::Apply);
    } else if (b==buttonBox->button(QDialogButtonBox::Cancel)) {
        slotButtonClicked(Dialog::Cancel);
    }
    #else
    Q_UNUSED(b)
    #endif
}

#include "moc_configdialog.cpp"
