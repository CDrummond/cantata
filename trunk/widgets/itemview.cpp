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
#include "groupedview.h"
#include "mainwindow.h"
#include "covers.h"
#include "proxymodel.h"
#include "actionitemdelegate.h"
#include "localize.h"
#include "icon.h"
#include "config.h"
#include <QtGui/QToolButton>
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

#ifdef ENABLE_KDE_SUPPORT
#define SINGLE_CLICK KGlobalSettings::singleClick()
#elif defined CANTATA_ANDROID
#define SINGLE_CLICK false
#else
#define SINGLE_CLICK style()->styleHint(QStyle::SH_ItemView_ActivateItemOnSingleClick, 0, this)
#endif

static int listDecorationSize=22;
static int treeDecorationSize=16;

void ItemView::setup()
{
    int height=QApplication::fontMetrics().height();

    if (height>22) {
        listDecorationSize=Icon::stdSize(height*1.4);
        treeDecorationSize=Icon::stdSize(height);
    } else {
        listDecorationSize=22;
        treeDecorationSize=16;
    }
}

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

#ifndef ENABLE_KDE_SUPPORT
Spinner::Spinner()
    : QWidget(0)
    , timer(0)
    , value(0)
{
    static const int constSize=24;
    setVisible(false);
    setMinimumSize(constSize, constSize);
    setMaximumSize(constSize, constSize);
}

void Spinner::start()
{
    value=0;
    setVisible(true);
    setPosition();
    if (!timer) {
        timer=new QTimer(this);
        connect(timer, SIGNAL(timeout()), this, SLOT(timeout()));
    }
    timer->start(100);
}

void Spinner::stop()
{
    setVisible(false);
    if (timer) {
        timer->stop();
    }
}

static const int constSpinnerSteps=64;

void Spinner::paintEvent(QPaintEvent *event)
{
    static const int constParts=8;
    QPainter p(this);
    QRectF rectangle(1.5, 1.5, size().width()-3, size().height()-3);
    QColor col(palette().color(QPalette::Text));
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setClipRect(event->rect());
    double size=(360*16)/(2.0*constParts);
    for (int i=0; i<constParts; ++i) {
        col.setAlphaF((constParts-i)/(1.0*constParts));
        p.setPen(QPen(col, 2));
        p.drawArc(rectangle, (((constSpinnerSteps-value)*1.0)/(constSpinnerSteps*1.0)*360*16)+(i*2.0*size), size);
    }
    p.end();
}

void Spinner::timeout()
{
    setPosition();
    update();
    if (++value>=constSpinnerSteps) {
        value=0;
    }
}

void Spinner::setPosition()
{
    QPoint current=pos();
    QPoint desired=QPoint(parentWidget()->size().width()-(size().width()+4), 4);

    if (current!=desired) {
        move(desired);
    }
}
#endif

static const int constImageSize=22;
static const int constDevImageSize=32;

static inline double subTextAlpha(bool selected)
{
    return selected ? 0.7 : 0.6;
}

class ListDelegate : public ActionItemDelegate
{
public:
    ListDelegate(ListView *v, QAbstractItemView *p, QAction *a1, QAction *a2, QAction *t, int actionLevel)
        : ActionItemDelegate(p, a1, a2, t, actionLevel)
        , view(v)
    {
    }

    virtual ~ListDelegate()
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        Q_UNUSED(option)
        int imageSize = index.data(ItemView::Role_ImageSize).toInt();

        if (imageSize<0) {
            imageSize=constImageSize;
        }

        if (view && QListView::IconMode==view->viewMode()) {
            // Use same calculation as in LibraryPage/AlbumsPage...
            return QSize(imageSize+8, imageSize+(QApplication::fontMetrics().height()*2.5));
        } else {
            bool oneLine = index.data(ItemView::Role_SubText).toString().isEmpty();
            bool showCapacity = !index.data(ItemView::Role_CapacityText).toString().isEmpty();
            int textHeight = QApplication::fontMetrics().height()*(oneLine ? 1 : 2);

            if (showCapacity) {
                imageSize=constDevImageSize;
            }
            return QSize(qMax(64, imageSize) + (constBorder * 2),
                         qMax(textHeight, imageSize) + (constBorder*2) + (int)((showCapacity ? textHeight*0.8 : 0)+0.5));
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

        QPixmap pix = QVariant::Pixmap==image.type() ? image.value<QPixmap>() : image.value<QIcon>().pixmap(listDecorationSize, listDecorationSize);
        bool oneLine = childText.isEmpty();
        bool iconMode = view && QListView::IconMode==view->viewMode();
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
            color.setAlphaF(subTextAlpha(selected));
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

private:
    ListView *view;
};

class TreeDelegate : public ListDelegate
{
public:
    TreeDelegate(QAbstractItemView *p, QAction *a1, QAction *a2, QAction *t, int actionLevel)
        : ListDelegate(0, p, a1, a2, t, actionLevel)
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

        QStringList text=index.data(Qt::DisplayRole).toString().split("\n");
        bool rtl = Qt::RightToLeft==QApplication::layoutDirection();

        if (1==text.count()) {
             QStyledItemDelegate::paint(painter, option, index);
        } else {
            QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);

            QRect r(option.rect);
            r.adjust(4, 0, -4, 0);
            QPixmap pix=index.data(Qt::DecorationRole).value<QIcon>().pixmap(treeDecorationSize, treeDecorationSize);
            if (!pix.isNull()) {
                int adjust=qMax(pix.width(), pix.height());
                if (rtl) {
                    painter->drawPixmap(r.x()+r.width()-pix.width(), r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
                    r.adjust(3, 0, -(3+adjust), 0);
                } else {
                    painter->drawPixmap(r.x(), r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
                    r.adjust(adjust+3, 0, -3, 0);
                }
            }

            if (text.count()>0) {
                QFont textFont(QApplication::font());
                QFontMetrics textMetrics(textFont);
                int textHeight=textMetrics.height();
                bool selected=option.state&QStyle::State_Selected;
                QColor color(option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text));
                QTextOption textOpt(Qt::AlignVCenter);
                QRect textRect(r.x(), r.y()+((r.height()-textHeight)/2), r.width(), textHeight);
                QString str=textMetrics.elidedText(text.at(0), Qt::ElideRight, textRect.width(), QPalette::WindowText);

                painter->save();
                painter->setPen(color);
                painter->setFont(textFont);
                painter->drawText(textRect, str, textOpt);

                if (text.count()>1) {
                    int mainWidth=textMetrics.width(str);
                    text.takeFirst();
                    str=text.join(" - ");
                    textRect=QRect(r.x()+(mainWidth+8), r.y()+((r.height()-textHeight)/2), r.width()-(mainWidth+8), textHeight);
                    if (textRect.width()>4) {
                        str = textMetrics.elidedText(str, Qt::ElideRight, textRect.width(), QPalette::WindowText);
                        color.setAlphaF(subTextAlpha(selected));
                        painter->setPen(color);
                        painter->drawText(textRect, str, textOpt/*QTextOption(Qt::AlignVCenter|Qt::AlignRight)*/);
                    }
                }
                painter->restore();
            }
        }

        if ((option.state & QStyle::State_MouseOver) && hasActions(index, actLevel)) {
            drawIcons(painter, option.rect, true, rtl, false, index);
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
    , groupedView(0)
    , spinnerActive(false)
    , spinner(0)
{
    setupUi(this);
    backAction = new QAction(i18n("Back"), this);
    backAction->setIcon(Icon("go-previous"));
    backButton->setDefaultAction(backAction);
    Icon::init(backButton);
    treeView->setPageDefaults();
    iconGridSize=listGridSize=listView->gridSize();
    listView->installEventFilter(new EscapeKeyEventHandler(listView, backAction));
}

ItemView::~ItemView()
{
}

void ItemView::allowGroupedView()
{
    if (!groupedView) {
        groupedView=new GroupedView(stackedWidget);
        groupedView->setAutoExpand(false);
        treeLayout->addWidget(groupedView);
        connect(groupedView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
        if (SINGLE_CLICK) {
            connect(groupedView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
        }
        connect(groupedView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
        connect(groupedView, SIGNAL(clicked(const QModelIndex &)), this, SLOT(itemClicked(const QModelIndex &)));
    }
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
    listView->setItemDelegate(new ListDelegate(listView, listView, a1, a2, toggle, actionLevel));
    treeView->setItemDelegate(new TreeDelegate(treeView, a1, a2, toggle, actionLevel));
    if (groupedView) {
        groupedView->init(0, 0, 0, 0); // No actions in grouped view :-(
    }
    connect(treeSearch, SIGNAL(returnPressed()), this, SLOT(delaySearchItems()));
    connect(treeSearch, SIGNAL(textChanged(const QString)), this, SLOT(delaySearchItems()));
    connect(listSearch, SIGNAL(returnPressed()), this, SLOT(delaySearchItems()));
    connect(listSearch, SIGNAL(textChanged(const QString)), this, SLOT(delaySearchItems()));
    connect(treeView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
    if (SINGLE_CLICK) {
        connect(treeView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    }
    connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
    connect(treeView, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
    connect(listView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
    connect(listView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(listView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
    connect(listView, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
    connect(backAction, SIGNAL(triggered(bool)), this, SLOT(backActivated()));
    #ifdef CANTATA_ANDROID
    connect(listView, SIGNAL(goBack()),  this, SLOT(backActivated()));
    #endif
    mode=Mode_List;
    setMode(Mode_Tree);
}

void ItemView::addAction(QAction *act)
{
    treeView->addAction(act);
    listView->addAction(act);
    if (groupedView) {
        groupedView->addAction(act);
    }
}

void ItemView::setMode(Mode m)
{
    if (m<0 || m>Mode_GroupedTree || (Mode_GroupedTree==m && !groupedView)) {
        m=Mode_Tree;
    }

    if (m==mode) {
        return;
    }

    mode=m;
    treeSearch->setText(QString());
    listSearch->setText(QString());
    if (Mode_Tree==mode) {
        treeView->setModel(itemModel);
        listView->setModel(0);
        if (groupedView) {
            groupedView->setHidden(true);
            groupedView->setModel(0);
        }
        treeView->setHidden(false);
        itemModel->setRootIndex(QModelIndex());
    } else if (Mode_GroupedTree==mode) {
        treeView->setModel(0);
        listView->setModel(0);
        groupedView->setHidden(false);
        treeView->setHidden(true);
        groupedView->setModel(itemModel);
        itemModel->setRootIndex(QModelIndex());
    } else {
        treeView->setModel(0);
        if (groupedView) {
            groupedView->setModel(0);
        }
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

    if (Mode_GroupedTree==mode) {
        connect(Covers::self(), SIGNAL(coverRetrieved(const Song &)), groupedView, SLOT(coverRetrieved(const Song &)));
    } else if (groupedView) {
        disconnect(Covers::self(), SIGNAL(coverRetrieved(const Song &)), groupedView, SLOT(coverRetrieved(const Song &)));
    }

    stackedWidget->setCurrentIndex(Mode_Tree==mode || Mode_GroupedTree==mode ? 0 : 1);
    if (spinner) {
        spinner->setWidget(view()->viewport());
        if (spinnerActive) {
            spinner->start();
        }
    }
}

void ItemView::hideBackButton()
{
    backButton->setVisible(false);
    listLayout->removeWidget(backButton);
    listLayout->removeWidget(listSearch);
    listLayout->removeWidget(listView);
    listLayout->addWidget(listSearch, 0, 0, 1, 1);
    listLayout->addWidget(listView, 1, 0, 1, 1);
}

QModelIndexList ItemView::selectedIndexes() const
{
    if (Mode_Tree==mode) {
        return treeView->selectedIndexes();
    } else if(Mode_GroupedTree==mode) {
        return groupedView->selectedIndexes();
    }
    return listView->selectedIndexes();
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
        listSearch->setPlaceholderText(i18n("Search %1...").arg(topText));
    } else if (prev>currentLevel) {
        listSearch->setPlaceholderText(prevText[currentLevel]);
    } else {
        prevText.insert(prev, listSearch->placeholderText());
    }
}

QAbstractItemView * ItemView::view() const
{
    if (Mode_Tree==mode) {
        return treeView;
    } else if(Mode_GroupedTree==mode) {
        return groupedView;
    } else {
        return listView;
    }
}

void ItemView::setModel(ProxyModel *m)
{
    itemModel=m;
    view()->setModel(m);
}

QString ItemView::searchText() const
{
    return Mode_Tree==mode || Mode_GroupedTree==mode ? treeSearch->text() : listSearch->text();
}

void ItemView::clearSearchText()
{
    return Mode_Tree==mode || Mode_GroupedTree==mode ? treeSearch->setText(QString()) : listSearch->setText(QString());
}

void ItemView::setTopText(const QString &text)
{
    topText=text;
    if (0==currentLevel) {
        listSearch->setPlaceholderText(i18n("Search %1...").arg(topText));
    }
    treeSearch->setPlaceholderText(i18n("Search %1...").arg(topText));
}

void ItemView::setUniformRowHeights(bool v)
{
    treeView->setUniformRowHeights(v);
}

void ItemView::setAcceptDrops(bool v)
{
    listView->setAcceptDrops(v);
    treeView->setAcceptDrops(v);
    if (groupedView) {
        groupedView->setAcceptDrops(v);
    }
}

void ItemView::setDragDropOverwriteMode(bool v)
{
    listView->setDragDropOverwriteMode(v);
    treeView->setDragDropOverwriteMode(v);
    if (groupedView) {
        groupedView->setDragDropOverwriteMode(v);
    }
}

void ItemView::setDragDropMode(QAbstractItemView::DragDropMode v)
{
    listView->setDragDropMode(v);
    treeView->setDragDropMode(v);
    if (groupedView) {
        groupedView->setDragDropMode(v);
    }
}

void ItemView::setGridSize(const QSize &sz)
{
    iconGridSize=sz;
    if (Mode_IconTop==mode && 0==currentLevel) {
        listView->setGridSize(listGridSize);
    }
}

void ItemView::update()
{
    if (Mode_Tree==mode) {
        treeView->update();
    } else if (Mode_GroupedTree==mode) {
        groupedView->update();
    } else {
        listView->update();
    }
}

void ItemView::setDeleteAction(QAction *act)
{
    listView->installEventFilter(new DeleteKeyEventHandler(listView, act));
    treeView->installEventFilter(new DeleteKeyEventHandler(treeView, act));
    if (groupedView) {
        groupedView->installEventFilter(new DeleteKeyEventHandler(groupedView, act));
    }
}

void ItemView::showIndex(const QModelIndex &idx, bool scrollTo)
{
    if (Mode_Tree==mode) {
        QModelIndex i=idx;
        while (i.isValid()) {
            treeView->setExpanded(i, true);
            i=i.parent();
        }
        if (scrollTo) {
            treeView->scrollTo(idx, QAbstractItemView::PositionAtTop);
        }
    } else if (Mode_GroupedTree==mode) {
        QModelIndex i=idx;
        while (i.isValid()) {
            groupedView->setExpanded(i, true);
            i=i.parent();
        }
        if (scrollTo) {
            groupedView->scrollTo(idx, QAbstractItemView::PositionAtTop);
        }
    } else {
        if (idx.parent().isValid()) {
            QList<QModelIndex> indexes;
            QModelIndex i=idx.parent();
            while (i.isValid()) {
                indexes.prepend(i);
                i=i.parent();
            }
            setLevel(0);
            listView->setRootIndex(QModelIndex());
            foreach (const QModelIndex &i, indexes) {
                itemActivated(i);
            }
            if (scrollTo) {
                listView->scrollTo(idx, QAbstractItemView::PositionAtTop);
            }
        }
    }

    view()->selectionModel()->select(idx, QItemSelectionModel::Select|QItemSelectionModel::Rows);
}

void ItemView::focusSearch()
{
    if (Mode_Tree==mode || Mode_GroupedTree==mode) {
        treeSearch->setFocus();
    } else {
        listSearch->setFocus();
    }
}

void ItemView::setStartClosed(bool sc)
{
    if (groupedView) {
        groupedView->setStartClosed(sc);
    }
}

bool ItemView::isStartClosed()
{
    return groupedView ? groupedView->isStartClosed() : false;
}

void ItemView::updateRows()
{
    if (groupedView) {
        groupedView->updateCollectionRows();
    }
}

void ItemView::updateRows(const QModelIndex &idx)
{
    if (groupedView) {
        groupedView->updateRows(idx);
    }
}

void ItemView::expandAll()
{
    if (Mode_Tree==mode) {
        treeView->expandAll();
    } else if (Mode_GroupedTree==mode && groupedView) {
        groupedView->expandAll();
    }
}

void ItemView::showSpinner()
{
    if (!spinner) {
        #ifdef ENABLE_KDE_SUPPORT
        spinner=new KPixmapSequenceOverlayPainter(this);
        spinner->setSequence(KPixmapSequence("process-working", KIconLoader::SizeSmallMedium));
        #else
        spinner=new Spinner();
        #endif
    }
    spinnerActive=true;
    spinner->setWidget(view()->viewport());
    #ifdef ENABLE_KDE_SUPPORT
    spinner->setAlignment(Qt::AlignTop | (Qt::RightToLeft==QApplication::layoutDirection() ? Qt::AlignLeft : Qt::AlignRight));
    #endif
    spinner->start();
}

void ItemView::hideSpinner()
{
    if (spinner) {
        spinnerActive=false;
        spinner->stop();
    }
}

void ItemView::collectionRemoved(quint32 key)
{
    if (groupedView) {
        groupedView->collectionRemoved(key);
    }
}

void ItemView::backActivated()
{
    if (Mode_Tree==mode || Mode_GroupedTree==mode || 0==currentLevel) {
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
            rect.adjust(ActionItemDelegate::constBorder, ActionItemDelegate::constBorder, -ActionItemDelegate::constBorder, -ActionItemDelegate::constBorder);
        } else {
            rect.adjust(ActionItemDelegate::constBorder+3, 0, -(ActionItemDelegate::constBorder+3), 0);
        }
    }

    if (showCapacity) {
        int textHeight=QFontMetrics(QApplication::font()).height();
        rect.adjust(0, 0, 0, -(textHeight+8));
    }

    QRect actionRect=ActionItemDelegate::calcActionRect(rtl, iconMode, rect);
    QRect actionRect2(actionRect);
    ActionItemDelegate::adjustActionRect(rtl, iconMode, actionRect2);

    if (act1 && actionRect.contains(QCursor::pos())) {
        return act1;
    }

    if (act1) {
        ActionItemDelegate::adjustActionRect(rtl, iconMode, actionRect);
    }

    if (act2 && actionRect.contains(QCursor::pos())) {
        return act2;
    }

    if (haveToggle) {
        if (act1 || act2) {
            ActionItemDelegate::adjustActionRect(rtl, iconMode, actionRect);
        }

        if (toggle && actionRect.contains(QCursor::pos())) {
            return toggle;
        }
    }

    return 0;
}

void ItemView::itemClicked(const QModelIndex &index)
{
    if ((act1 || act2 || toggle) && ActionItemDelegate::hasActions(index, actLevel)) {
        QAction *act=getAction(index);
        if (act) {
            act->trigger();
            return;
        }
    }
    #ifdef CANTATA_ANDROID
    itemActivated(index);
    #endif
}

void ItemView::itemActivated(const QModelIndex &index)
{
    if ((act1 || act2 || toggle) && ActionItemDelegate::hasActions(index, actLevel) && getAction(index)) {
        return;
    }

    if (Mode_Tree==mode) {
        treeView->setExpanded(index, !treeView->isExpanded(index));
    } else if (Mode_GroupedTree==mode) {
        if (!index.parent().isValid()) {
            groupedView->setExpanded(index, !groupedView->TreeView::isExpanded(index));
        }
    } else if (index.isValid() && index.child(0, 0).isValid()) {
        prevTopIndex=listView->indexAt(QPoint(8, 8));
        if (qobject_cast<QSortFilterProxyModel *>(listView->model())) {
            prevTopIndex=static_cast<QSortFilterProxyModel *>(listView->model())->mapToSource(prevTopIndex);
        }
        setLevel(currentLevel+1, index.child(0, 0).child(0, 0).isValid());
        QString text=index.data(Role_Search).toString();
        if (text.isEmpty()) {
            text=index.data(Qt::DisplayRole).toString();
        }
        listSearch->setPlaceholderText(i18n("Search %1...").arg(text));
        listView->setRootIndex(index);
        itemModel->setRootIndex(index);
        listView->scrollToTop();
    }
}

void ItemView::delaySearchItems()
{
    bool isTree=Mode_Tree==mode || Mode_GroupedTree==mode;
    if ((isTree && treeSearch->text().isEmpty()) || (!isTree && listSearch->text().isEmpty())) {
        if (searchTimer) {
            searchTimer->stop();
        }
        emit searchItems();
    } else {
        if (!searchTimer) {
            searchTimer=new QTimer(this);
            searchTimer->setSingleShot(true);
            connect(searchTimer, SIGNAL(timeout()), SIGNAL(searchItems()));
        }
        searchTimer->start(500);
    }
}
