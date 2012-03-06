/*
 * Cantata
 *
 * Copyright (c) 2011-2012 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "itemview.h"
#include "mainwindow.h"
#include "covers.h"
#include "proxymodel.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QStyle>
#include <QtGui/QStyleOptionViewItem>
#include <QtGui/QPainter>
#include <QtGui/QAction>
#include <QtCore/QTimer>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobalSettings>
#include <KDE/KPixmapSequence>
#include <KDE/KPixmapSequenceOverlayPainter>
#endif

EscapeKeyEventHandler::EscapeKeyEventHandler(QAbstractItemView *v, QAction *a)
    : QObject(v)
    , view(v)
    , act(a)
{
}

bool EscapeKeyEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    if (view->hasFocus() && QEvent::KeyRelease==event->type() && static_cast<QKeyEvent *>(event)->key()==Qt::Key_Escape) {
        if (act->isEnabled()) {
            act->trigger();
        }
        return true;
    }
    return QObject::eventFilter(obj, event);
}

// static QPainterPath buildPath(const QRectF &r, double radius)
// {
//     QPainterPath path;
//     double diameter(radius*2);
//
//     path.moveTo(r.x()+r.width(), r.y()+r.height()-radius);
//     path.arcTo(r.x()+r.width()-diameter, r.y(), diameter, diameter, 0, 90);
//     path.arcTo(r.x(), r.y(), diameter, diameter, 90, 90);
//     path.arcTo(r.x(), r.y()+r.height()-diameter, diameter, diameter, 180, 90);
//     path.arcTo(r.x()+r.width()-diameter, r.y()+r.height()-diameter, diameter, diameter, 270, 90);
//     return path;
// }

static void drawBgnd(QPainter *painter, const QRect &rx)
{
    QRectF r(rx.x()-0.5, rx.y()-0.5, rx.width()+1, rx.height()+1);
    QPainterPath p;//(buildPath(r, r.width()/2.0));
    QColor c(Qt::white);

    p.addEllipse(r);
    painter->setRenderHint(QPainter::Antialiasing, true);
    c.setAlphaF(0.75);
    painter->fillPath(p, c);
//     c.setAlphaF(0.95);
//     painter->setPen(c);
//     painter->drawPath(p);
    painter->setRenderHint(QPainter::Antialiasing, false);
}

static const int constBorder = 1;
static const int constActionBorder = 4;
static const int constActionIconSize=16;
static const int constImageSize=22;
static const int constDevImageSize=32;

QRect calcActionRect(bool rtl, bool iconMode, const QRect &rect)
{
    return rtl
                ? iconMode
                    ? QRect(rect.x()+constActionBorder,
                            rect.y()+constActionBorder,
                            constActionIconSize, constActionIconSize)
                    : QRect(rect.x()+constActionBorder,
                            rect.y()+((rect.height()-constActionIconSize)/2),
                            constActionIconSize, constActionIconSize)
                : iconMode
                    ? QRect(rect.x()+rect.width()-(constActionIconSize+constActionBorder),
                            rect.y()+constActionBorder,
                            constActionIconSize, constActionIconSize)
                    : QRect(rect.x()+rect.width()-(constActionIconSize+constActionBorder),
                            rect.y()+((rect.height()-constActionIconSize)/2),
                            constActionIconSize, constActionIconSize);
}

static void adjustActionRect(bool rtl, bool iconMode, QRect &rect)
{
    if (rtl) {
        if (iconMode) {
            rect.adjust(0, constActionIconSize+constActionBorder, 0, constActionIconSize+constActionBorder);
        } else {
            rect.adjust(constActionIconSize+constActionBorder, 0, constActionIconSize+constActionBorder, 0);
        }
    } else {
        if (iconMode) {
            rect.adjust(0, constActionIconSize+constActionBorder, 0, constActionIconSize+constActionBorder);
        } else {
            rect.adjust(-(constActionIconSize+constActionBorder), 0, -(constActionIconSize+constActionBorder), 0);
        }
    }
}

static bool hasActions(const QModelIndex &index, int actLevel)
{
    if (actLevel<0) {
        return true;
    }

    int level=0;

    QModelIndex idx=index;
    while(idx.parent().isValid()) {
        if (++level>actLevel) {
            return false;
        }
        idx=idx.parent();
    }
    return true;
}

class ListDelegate : public QStyledItemDelegate
{
public:
    ListDelegate(QObject *p, QAction *a1, QAction *a2, QAction *t, int actionLevel)
        : QStyledItemDelegate(p)
        , act1(a1)
        , act2(a2)
        , toggle(t)
        , actLevel(actionLevel)
    {
    }

    virtual ~ListDelegate()
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        QSize sz(QStyledItemDelegate::sizeHint(option, index));
        int imageSize = index.data(ItemView::Role_ImageSize).toInt();

        if (imageSize<0) {
            imageSize=constImageSize;
        }

        if (imageSize>50) { // Icon Mode!
            int textHeight = QApplication::fontMetrics().height()*2;
            return QSize(imageSize + (constBorder * 2), textHeight+imageSize + (constBorder*2));
        } else {
            bool oneLine = index.data(ItemView::Role_SubText).toString().isEmpty();
            bool showCapacity = !index.data(ItemView::Role_CapacityText).toString().isEmpty();
            int textHeight = QApplication::fontMetrics().height()*(oneLine ? 1 : 2);

            if (showCapacity) {
                imageSize=constDevImageSize;
            }
            return QSize(qMax(64, imageSize) + (constBorder * 2),
                        qMax(qMax(textHeight, imageSize) + (constBorder*2) + (int)((showCapacity ? textHeight*0.8 : 0)+0.5), sz.height()));
        }
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);

        QString capacityText=index.data(ItemView::Role_CapacityText).toString();
        bool showCapacity = !capacityText.isEmpty();
        QString text = index.data(ItemView::Role_MainText).toString();
        if (text.isEmpty()) {
            text=index.data(Qt::DisplayRole).toString();
        }
        QRect r(option.rect);
        QRect r2(r);
        QString childText = index.data(ItemView::Role_SubText).toString();
        int imageSize = showCapacity ? constDevImageSize : index.data(ItemView::Role_ImageSize).toInt();

        if (imageSize<=0) {
            imageSize=constImageSize;
        }

        QVariant image = index.data(ItemView::Role_Image);
        if (image.isNull()) {
            image = index.data(Qt::DecorationRole);
        }

        QPixmap pix = QVariant::Pixmap==image.type() ? image.value<QPixmap>() : image.value<QIcon>().pixmap(imageSize, imageSize);
        bool oneLine = childText.isEmpty();
        bool iconMode = imageSize>50;
        bool rtl = Qt::RightToLeft==QApplication::layoutDirection();

        painter->save();
        painter->setClipRect(r);

        QFont textFont(QApplication::font());
        QFontMetrics textMetrics(textFont);
        int textHeight=textMetrics.height();

        if (showCapacity) {
            r.adjust(2, 0, 0, -(textHeight+4));
        }

        if (iconMode) {
            r.adjust(constBorder, constBorder, -constBorder, -constBorder);
            r2=r;
        } else {
            r.adjust(constBorder, 0, -constBorder, 0);
        }
        if (!pix.isNull()) {
            int adjust=qMax(pix.width(), pix.height());
            if (iconMode) {
                painter->drawPixmap(r.x()+((r.width()-pix.width())/2), r.y(), pix.width(), pix.height(), pix);
                r.adjust(0, adjust+3, 0, -3);
            } else {
                if (rtl) {
                    painter->drawPixmap(r.x()+r.width()-pix.width(), r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
                    r.adjust(3, 0, -(3+adjust), 0);
                } else {
                    painter->drawPixmap(r.x(), r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
                    r.adjust(adjust+3, 0, -3, 0);
                }
            }
        }

        if (!(option.state & QStyle::State_MouseOver) && hasActions(index, actLevel)) {
            drawIcons(painter, iconMode ? r2 : r, false, rtl, iconMode, index);
        }

        QRect textRect;
        bool selected=option.state&QStyle::State_Selected;
        QColor color(option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));
        QTextOption textOpt(iconMode ? Qt::AlignHCenter|Qt::AlignVCenter : Qt::AlignVCenter);

        if (oneLine) {
            textRect=QRect(r.x(), r.y()+((r.height()-textHeight)/2), r.width(), textHeight);
            text = textMetrics.elidedText(text, Qt::ElideRight, textRect.width(), QPalette::WindowText);
            painter->setPen(color);
            painter->setFont(textFont);
            painter->drawText(textRect, text, textOpt);
        } else {
            QFont childFont(QApplication::font());
            childFont.setPointSizeF(childFont.pointSizeF()*0.8);
            QFontMetrics childMetrics(childFont);
            QRect childRect;

            int childHeight=childMetrics.height();
            int totalHeight=textHeight+childHeight;
            textRect=QRect(r.x(), r.y()+((r.height()-totalHeight)/2), r.width(), textHeight);
            childRect=QRect(r.x(), r.y()+textHeight+((r.height()-totalHeight)/2), r.width(), textHeight);

            text = textMetrics.elidedText(text, Qt::ElideRight, textRect.width(), QPalette::WindowText);
            painter->setPen(color);
            painter->setFont(textFont);
            painter->drawText(textRect, text, textOpt);

            childText = childMetrics.elidedText(childText, Qt::ElideRight, childRect.width(), QPalette::WindowText);
            color.setAlphaF(selected ? 0.8 : 0.7);
            painter->setPen(color);
            painter->setFont(childFont);
            painter->drawText(childRect, childText, textOpt);
        }

        if (showCapacity) {
            QStyleOptionProgressBar opt;
            double capacity=index.data(ItemView::Role_Capacity).toDouble();

            opt.minimum=0;
            opt.maximum=1000;
            opt.progress=capacity<0 ? 0 : (index.data(ItemView::Role_Capacity).toDouble()*1000);
            opt.textVisible=true;
            opt.text=capacityText;
            opt.rect=QRect(r2.x()+4, r2.bottom()-(textHeight+4), r2.width()-8, textHeight);
            opt.state=QStyle::State_Enabled;
            opt.palette=option.palette;
            opt.direction=QApplication::layoutDirection();
            opt.fontMetrics=textMetrics;

            QApplication::style()->drawControl(QStyle::CE_ProgressBar, &opt, painter, 0L);
        }

        if ((option.state & QStyle::State_MouseOver) && hasActions(index, actLevel)) {
            drawIcons(painter, iconMode ? r2 : r, true, rtl, iconMode, index);
        }

        painter->restore();
    }

    void drawIcons(QPainter *painter, const QRect &r, bool mouseOver, bool rtl, bool iconMode, const QModelIndex &index) const
    {
        double opacity=painter->opacity();
        if (!mouseOver) {
            painter->setOpacity(opacity*0.2);
        }

        QRect actionRect=calcActionRect(rtl, iconMode, r);
        if (act1) {
            QPixmap pix=act1->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
            if (!pix.isNull() && actionRect.width()>=pix.width()/* && r.x()>=0 && r.y()>=0*/) {
                drawBgnd(painter, actionRect);
                painter->drawPixmap(actionRect.x()+(actionRect.width()-pix.width())/2,
                                    actionRect.y()+(actionRect.height()-pix.height())/2, pix);
            }
        }

        if (act1 && act2) {
            adjustActionRect(rtl, iconMode, actionRect);
            QPixmap pix=act2->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
            if (!pix.isNull() && actionRect.width()>=pix.width()/* && r.x()>=0 && r.y()>=0*/) {
                drawBgnd(painter, actionRect);
                painter->drawPixmap(actionRect.x()+(actionRect.width()-pix.width())/2,
                                    actionRect.y()+(actionRect.height()-pix.height())/2, pix);
            }
        }

        if (act1 && act2 && toggle) {
            QString iconName=index.data(ItemView::Role_ToggleIconName).toString();

            if (!iconName.isEmpty()) {
                adjustActionRect(rtl, iconMode, actionRect);
                QPixmap pix=QIcon::fromTheme(iconName).pixmap(QSize(constActionIconSize, constActionIconSize));
                if (!pix.isNull() && actionRect.width()>=pix.width()/* && r.x()>=0 && r.y()>=0*/) {
                    drawBgnd(painter, actionRect);
                    painter->drawPixmap(actionRect.x()+(actionRect.width()-pix.width())/2,
                                        actionRect.y()+(actionRect.height()-pix.height())/2, pix);
                }
            }
        }
        if (!mouseOver) {
            painter->setOpacity(opacity);
        }
    }

    QAction *act1;
    QAction *act2;
    QAction *toggle;
    int actLevel;
};

class TreeDelegate : public ListDelegate
{
public:
    TreeDelegate(QObject *p, QAction *a1, QAction *a2, QAction *t, int actionLevel)
        : ListDelegate(p, a1, a2, t, actionLevel)
    {
    }

    virtual ~TreeDelegate()
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.data(ItemView::Role_CapacityText).toString().isEmpty()) {
            return ListDelegate::sizeHint(option, index);
        }

        QSize sz(QStyledItemDelegate::sizeHint(option, index));

        return QSize(sz.width(), sz.height()+2);
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }

        if(!index.data(ItemView::Role_CapacityText).toString().isEmpty()) {
            ListDelegate::paint(painter, option, index);
            return;
        }

        QStyledItemDelegate::paint(painter, option, index);

        if ((option.state & QStyle::State_MouseOver) && hasActions(index, actLevel)) {
            drawIcons(painter, option.rect, true, Qt::RightToLeft==QApplication::layoutDirection(), false, index);
        }
    }
};

ItemView::ItemView(QWidget *p)
    : QWidget(p)
    , searchTimer(0)
    , itemModel(0)
    , actLevel(-1)
    , act1(0)
    , act2(0)
    , toggle(0)
    , currentLevel(0)
    , mode(Mode_Tree)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    spinner=0;
    spinnerActive=false;
    backAction = new QAction(i18n("Back"), this);
    #else
    backAction = new QAction(tr("Back"), this);
    #endif
    backAction->setIcon(QIcon::fromTheme("go-previous"));
    backButton->setDefaultAction(backAction);
    backButton->setAutoRaise(true);
    treeView->setPageDefaults();
    iconGridSize=listGridSize=listView->gridSize();
    listView->installEventFilter(new EscapeKeyEventHandler(listView, backAction));
}

ItemView::~ItemView()
{
}

void ItemView::init(QAction *a1, QAction *a2, QAction *t, int actionLevel)
{
    if (act1 || act2 || toggle) {
        return;
    }

    act1=a1;
    act2=a2;
    toggle=t;
    actLevel=actionLevel;
    listView->setItemDelegate(new ListDelegate(this, a1, a2, toggle, actionLevel));
    treeView->setItemDelegate(new TreeDelegate(this, a1, a2, toggle, actionLevel));
    connect(treeSearch, SIGNAL(returnPressed()), this, SLOT(delaySearchItems()));
    connect(treeSearch, SIGNAL(textChanged(const QString)), this, SLOT(delaySearchItems()));
    connect(listSearch, SIGNAL(returnPressed()), this, SLOT(delaySearchItems()));
    connect(listSearch, SIGNAL(textChanged(const QString)), this, SLOT(delaySearchItems()));
    connect(treeView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
#ifdef ENABLE_KDE_SUPPORT
    if (KGlobalSettings::singleClick()) {
        connect(treeView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    }
#endif
    connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
    connect(treeView, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
    connect(listView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
    connect(listView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(listView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
    connect(listView, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
    connect(backAction, SIGNAL(triggered(bool)), this, SLOT(backActivated()));
    mode=Mode_List;
    setMode(Mode_Tree);
}

void ItemView::addAction(QAction *act)
{
    treeView->addAction(act);
    listView->addAction(act);
}

void ItemView::setMode(Mode m)
{
    if (m==mode) {
        return;
    }

    mode=m;
    treeSearch->setText(QString());
    listSearch->setText(QString());
    if (Mode_Tree==mode) {
        treeView->setModel(itemModel);
        listView->setModel(0);
        itemModel->setRootIndex(QModelIndex());
    } else {
        treeView->setModel(0);
        listView->setModel(itemModel);
        setLevel(0);
        listView->setRootIndex(QModelIndex());
        itemModel->setRootIndex(QModelIndex());
        if (Mode_IconTop!=mode) {
            listView->setGridSize(listGridSize);
            listView->setViewMode(QListView::ListMode);
            listView->setResizeMode(QListView::Fixed);
            listView->setAlternatingRowColors(true);
            listView->setWordWrap(false);
        }
    }
    stackedWidget->setCurrentIndex(Mode_Tree==mode ? 0 : 1);
    #ifdef ENABLE_KDE_SUPPORT
    if (spinner) {
        spinner->setWidget(view()->viewport());
        if (spinnerActive) {
            spinner->start();
        }
    }
    #endif
}

QModelIndexList ItemView::selectedIndexes() const
{
    return Mode_Tree==mode ? treeView->selectedIndexes() : listView->selectedIndexes();
}

void ItemView::setLevel(int l, bool haveChildren)
{
    int prev=currentLevel;
    currentLevel=l;
    backAction->setEnabled(0!=l);

    if (Mode_IconTop==mode) {
        if (0==currentLevel || haveChildren) {
            if (QListView::IconMode!=listView->viewMode()) {
                listView->setGridSize(iconGridSize);
                listView->setViewMode(QListView::IconMode);
                listView->setResizeMode(QListView::Adjust);
                listView->setAlternatingRowColors(false);
                listView->setWordWrap(true);
                listView->setDragDropMode(QAbstractItemView::DragOnly);
            }
        } else if(QListView::ListMode!=listView->viewMode()) {
            listView->setGridSize(listGridSize);
            listView->setViewMode(QListView::ListMode);
            listView->setResizeMode(QListView::Fixed);
            listView->setAlternatingRowColors(true);
            listView->setWordWrap(false);
            listView->setDragDropMode(QAbstractItemView::DragOnly);
        }
    }

    if (view()->selectionModel()) {
        view()->selectionModel()->clearSelection();
    }

    if (0==currentLevel) {
        #ifdef ENABLE_KDE_SUPPORT
        listSearch->setPlaceholderText(i18n("Search %1...", topText));
        #else
        listSearch->setPlaceholderText(tr("Search %1...").arg(topText));
        #endif
    } else if (prev>currentLevel) {
        listSearch->setPlaceholderText(prevText[currentLevel]);
    } else {
        prevText.insert(prev, listSearch->placeholderText());
    }
}

QAbstractItemView * ItemView::view() const
{
    return Mode_Tree==mode ? (QAbstractItemView *)treeView : (QAbstractItemView *)listView;
}

void ItemView::setModel(ProxyModel *m)
{
    itemModel=m;
    view()->setModel(m);
}

QString ItemView::searchText() const
{
    return Mode_Tree==mode ? treeSearch->text() : listSearch->text();
}

void ItemView::setTopText(const QString &text)
{
    topText=text;
    if (0==currentLevel) {
        #ifdef ENABLE_KDE_SUPPORT
        listSearch->setPlaceholderText(i18n("Search %1...", topText));
        #else
        listSearch->setPlaceholderText(tr("Search %1...").arg(topText));
        #endif
    }
    #ifdef ENABLE_KDE_SUPPORT
    treeSearch->setPlaceholderText(i18n("Search %1...", topText));
    #else
    treeSearch->setPlaceholderText(tr("Search %1...").arg(topText));
    #endif
}

void ItemView::setUniformRowHeights(bool v)
{
    treeView->setUniformRowHeights(v);
}

void ItemView::setAcceptDrops(bool v)
{
    listView->setAcceptDrops(v);
    treeView->setAcceptDrops(v);
}

void ItemView::setDragDropOverwriteMode(bool v)
{
    listView->setDragDropOverwriteMode(v);
    treeView->setDragDropOverwriteMode(v);
}

void ItemView::setDragDropMode(QAbstractItemView::DragDropMode v)
{
    listView->setDragDropMode(v);
    treeView->setDragDropMode(v);
}

void ItemView::setGridSize(const QSize &sz)
{
    iconGridSize=sz;
}

void ItemView::setDeleteAction(QAction *act)
{
    listView->installEventFilter(new DeleteKeyEventHandler(listView, act));
    treeView->installEventFilter(new DeleteKeyEventHandler(treeView, act));
}

void ItemView::showSpinner()
{
    #ifdef ENABLE_KDE_SUPPORT
    if (!spinner) {
        spinner=new KPixmapSequenceOverlayPainter(this);
        spinner->setSequence(KPixmapSequence("process-working", KIconLoader::SizeSmallMedium));
    }
    spinnerActive=true;
    spinner->setWidget(view()->viewport());
    spinner->setAlignment(Qt::AlignTop | (Qt::RightToLeft==QApplication::layoutDirection() ? Qt::AlignLeft : Qt::AlignRight));
    spinner->start();
    #endif
}

void ItemView::hideSpinner()
{
    #ifdef ENABLE_KDE_SUPPORT
    if (spinner) {
        spinnerActive=false;
        spinner->stop();
    }
    #endif
}

void ItemView::backActivated()
{
    if (Mode_Tree==mode) {
        return;
    }
    setLevel(currentLevel-1);
    itemModel->setRootIndex(listView->rootIndex().parent());
    listView->setRootIndex(listView->rootIndex().parent());

    if (qobject_cast<QSortFilterProxyModel *>(listView->model())) {
        QModelIndex idx=static_cast<QSortFilterProxyModel *>(listView->model())->mapFromSource(prevTopIndex);
        if (idx.isValid()) {
            listView->scrollTo(idx, QAbstractItemView::PositionAtTop);
        }
    } else {
        listView->scrollTo(prevTopIndex, QAbstractItemView::PositionAtTop);
    }
}

QAction * ItemView::getAction(const QModelIndex &index)
{
    bool rtl = Qt::RightToLeft==QApplication::layoutDirection();
    bool iconMode=Mode_IconTop==mode && index.child(0, 0).isValid();
    QRect rect(view()->visualRect(index));
    rect.moveTo(view()->viewport()->mapToGlobal(QPoint(rect.x(), rect.y())));
    bool showCapacity = !index.data(ItemView::Role_CapacityText).toString().isEmpty();
    bool haveToggle = toggle && !index.data(ItemView::Role_ToggleIconName).toString().isEmpty();
    if (Mode_Tree!=mode || showCapacity) {
        if (iconMode) {
            rect.adjust(constBorder, constBorder, -constBorder, -constBorder);
        } else {
            rect.adjust(constBorder+3, 0, -(constBorder+3), 0);
        }
    }

    if (showCapacity) {
        int textHeight=QFontMetrics(QApplication::font()).height();
        rect.adjust(0, 0, 0, -(textHeight+8));
    }

    QRect actionRect=calcActionRect(rtl, iconMode, rect);
    QRect actionRect2(actionRect);
    adjustActionRect(rtl, iconMode, actionRect2);

    if (act1 && actionRect.contains(QCursor::pos())) {
        return act1;
    }

    adjustActionRect(rtl, iconMode, actionRect);

    if (act2 && actionRect.contains(QCursor::pos())) {
        return act2;
    }

    if (haveToggle) {
        adjustActionRect(rtl, iconMode, actionRect);

        if (toggle && actionRect.contains(QCursor::pos())) {
            return toggle;
        }
    }

    return 0;
}

void ItemView::itemClicked(const QModelIndex &index)
{
    if (hasActions(index, actLevel)) {
        QAction *act=getAction(index);
        if (act) {
            act->trigger();
        }
    }
}

void ItemView::itemActivated(const QModelIndex &index)
{
    if (hasActions(index, actLevel)) {
        QAction *act=getAction(index);
        if (act) {
            return;
        }
    }

    if (Mode_Tree==mode) {
        treeView->setExpanded(index, !treeView->isExpanded(index));
    } else if (index.isValid() && index.child(0, 0).isValid()) {
        prevTopIndex=listView->indexAt(QPoint(0, 0));
        if (qobject_cast<QSortFilterProxyModel *>(listView->model())) {
            prevTopIndex=static_cast<QSortFilterProxyModel *>(listView->model())->mapToSource(prevTopIndex);
        }
        setLevel(currentLevel+1, index.child(0, 0).child(0, 0).isValid());
        QString text=index.data(Role_Search).toString();
        if (text.isEmpty()) {
            text=index.data(Qt::DisplayRole).toString();
        }
        #ifdef ENABLE_KDE_SUPPORT
        listSearch->setPlaceholderText(i18n("Search %1...", text));
        #else
        listSearch->setPlaceholderText(tr("Search %1...").arg(text));
        #endif
        listView->setRootIndex(index);
        itemModel->setRootIndex(index);
        listView->scrollToTop();
    }
}

void ItemView::delaySearchItems()
{
    if ((Mode_Tree==mode && treeSearch->text().isEmpty()) || (Mode_Tree!=mode && listSearch->text().isEmpty())) {
        if (searchTimer) {
            searchTimer->stop();
        }
        emit searchItems();
    } else {
        if (!searchTimer) {
            searchTimer=new QTimer(this);
            connect(searchTimer, SIGNAL(timeout()), SIGNAL(searchItems()));
        }
        searchTimer->start(500);
    }
}
