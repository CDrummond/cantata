/*
 * Cantata
 *
 * Copyright (c) 2011-2013 Craig Drummond <craig.p.drummond@gmail.com>
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
#include "actionmodel.h"
#include "localize.h"
#include "icon.h"
#include "config.h"
#include "gtkstyle.h"
#include "spinner.h"
#include <QToolButton>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QAction>
#include <QTimer>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobalSettings>
#endif

static bool forceSingleClick=true;

#ifdef ENABLE_KDE_SUPPORT
#define SINGLE_CLICK KGlobalSettings::singleClick()
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

ListViewEventHandler::ListViewEventHandler(QAbstractItemView *v, QAction *a)
    : QObject(v)
    , view(v)
    , act(a)
{
}

// HACK time. For some reason, IconView is not always re-drawn when mouse leaves the view.
// We sometimes get an item that is left in the mouse-over state. So, work-around this by
// keeping track of when mouse is over listview.
static QWidget *mouseWidget=0;

bool ListViewEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    if (view->hasFocus() && QEvent::KeyRelease==event->type() && static_cast<QKeyEvent *>(event)->key()==Qt::Key_Escape) {
        if (act->isEnabled()) {
            act->trigger();
        }
        return true;
    }
    if (QEvent::Enter==event->type()) {
        mouseWidget=view;
        view->viewport()->update();
    } else if (QEvent::Leave==event->type()) {
        mouseWidget=0;
        view->viewport()->update();
    }
    return QObject::eventFilter(obj, event);
}

static const int constImageSize=22;
static const int constDevImageSize=32;

static inline double subTextAlpha(bool selected)
{
    return selected ? 0.7 : 0.6;
}

class ListDelegate : public ActionItemDelegate
{
public:
    ListDelegate(ListView *v, QAbstractItemView *p)
        : ActionItemDelegate(p)
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
        bool mouseOver=option.state&QStyle::State_MouseOver;
        bool gtk=mouseOver && GtkStyle::isActive();
        bool selected=option.state&QStyle::State_Selected;
        bool active=option.state&QStyle::State_Active;
        bool drawBgnd=true;
        QStyleOptionViewItemV4 opt(option);
        opt.showDecorationSelected=true;

        if (view!=mouseWidget) {
            if (mouseOver && !selected) {
                drawBgnd=false;
            }
            mouseOver=false;
        }
        if (drawBgnd) {
            if (mouseOver && gtk) {
                GtkStyle::drawSelection(opt, painter, selected ? 0.75 : 0.25);
            } else {
                QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, 0L);
            }
        }

        QString capacityText=index.data(ItemView::Role_CapacityText).toString();
        bool showCapacity = !capacityText.isEmpty();
        QString text = index.data(ItemView::Role_MainText).toString();
        if (text.isEmpty()) {
            text=index.data(Qt::DisplayRole).toString();
        }
        QRect r(option.rect);
        QRect r2(r);
        QString childText = index.data(ItemView::Role_SubText).toString();
        QVariant image = index.data(ItemView::Role_Image);
        if (image.isNull()) {
            image = index.data(Qt::DecorationRole);
        }

        QPixmap pix = QVariant::Pixmap==image.type()
                        ? image.value<QPixmap>()
                        : QVariant::Image==image.type()
                            ? QPixmap::fromImage(image.value<QImage>().scaled(listDecorationSize, listDecorationSize,
                                                                              Qt::KeepAspectRatio, Qt::SmoothTransformation))
                            : image.value<QIcon>().pixmap(listDecorationSize, listDecorationSize);
        bool oneLine = childText.isEmpty();
        bool iconMode = view && QListView::IconMode==view->viewMode();
        ActionPos actionPos = iconMode ? AP_VTop : AP_HMiddle;
        bool rtl = Qt::RightToLeft==QApplication::layoutDirection();

        painter->save();
        painter->setClipRect(r);

        QFont textFont(QApplication::font());
        QFontMetrics textMetrics(textFont);
        int textHeight=textMetrics.height();

        if (showCapacity) {
            r.adjust(2, 0, 0, -(textHeight+4));
        }

        if (AP_VTop==actionPos) {
            r.adjust(constBorder, constBorder, -constBorder, -constBorder);
            r2=r;
        } else {
            r.adjust(constBorder, 0, -constBorder, 0);
        }
        if (!pix.isNull()) {
            int adjust=qMax(pix.width(), pix.height());
            if (AP_VTop==actionPos) {
                int xpos=r.x()+((r.width()-pix.width())/2);
                painter->drawPixmap(xpos, r.y(), pix.width(), pix.height(), pix);
                QColor color(option.palette.color(active ? QPalette::Active : QPalette::Inactive, QPalette::Text));
                double alphas[]={0.25, 0.125, 0.061};
                for (int i=0; i<3; ++i) {
                    color.setAlphaF(alphas[i]);
                    painter->setPen(color);
                    painter->drawLine(xpos+1+(i*2), r.y()+pix.height()+1+i, xpos+pix.width()-(i ? i : 1), r.y()+pix.height()+1+i);
                    painter->drawLine(xpos+pix.width()+1+i, r.y()+1+(i*2), xpos+pix.width()+1+i, r.y()+pix.height()-(i ? i : 1));
                    if (1==i) {
                        painter->drawPoint(xpos+pix.width()+1, r.y()+pix.height());
                        painter->drawPoint(xpos+pix.width(), r.y()+pix.height()+1);
                        color.setAlphaF(alphas[i+1]);
                        painter->setPen(color);
                        painter->drawPoint(xpos+pix.width()+1, r.y()+pix.height()+1);
                        painter->drawPoint(xpos+(i*2), r.y()+pix.height()+1+i);
                        painter->drawPoint(xpos+pix.width()+1+i, r.y()+(i*2));
                    } else if (2==i) {
                        painter->drawPoint(xpos+pix.width()+2, r.y()+pix.height());
                        painter->drawPoint(xpos+pix.width(), r.y()+pix.height()+2);
                    }
                }
                color.setAlphaF(0.4);
                painter->setPen(color);
                painter->drawRect(xpos, r.y(), pix.width(), pix.height());
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

        if (!mouseOver) {
            drawIcons(painter, AP_VTop==actionPos ? r2 : r, false, rtl, actionPos, index);
        }

        QRect textRect;
        QColor color(option.palette.color(active ? QPalette::Active : QPalette::Inactive, selected ? QPalette::HighlightedText : QPalette::Text));
        QTextOption textOpt(AP_VTop==actionPos ? Qt::AlignHCenter|Qt::AlignVCenter : Qt::AlignVCenter);

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

            if (capacity<0.0) {
                capacity=0.0;
            } else if (capacity>1.0) {
                capacity=1.0;
            }
            opt.minimum=0;
            opt.maximum=1000;
            opt.progress=capacity*1000;
            opt.textVisible=true;
            opt.text=capacityText;
            opt.rect=QRect(r2.x()+4, r2.bottom()-(textHeight+4), r2.width()-8, textHeight);
            opt.state=QStyle::State_Enabled;
            opt.palette=option.palette;
            opt.direction=QApplication::layoutDirection();
            opt.fontMetrics=textMetrics;

            QApplication::style()->drawControl(QStyle::CE_ProgressBar, &opt, painter, 0L);
        }

        if (drawBgnd && mouseOver) {
            drawIcons(painter, AP_VTop==actionPos ? r2 : r, true, rtl, actionPos, index);
        }

        painter->restore();
    }

protected:
    ListView *view;
};

class TreeDelegate : public ListDelegate
{
public:
    TreeDelegate(QAbstractItemView *p)
        : ListDelegate(0, p)
        , simpleStyle(true)
    {
    }

    virtual ~TreeDelegate()
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!simpleStyle || !index.data(ItemView::Role_CapacityText).toString().isEmpty()) {
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

        if (!simpleStyle || !index.data(ItemView::Role_CapacityText).toString().isEmpty()) {
            ListDelegate::paint(painter, option, index);
            return;
        }

        QStringList text=index.data(Qt::DisplayRole).toString().split("\n");
        bool gtk=GtkStyle::isActive();
        bool rtl = Qt::RightToLeft==QApplication::layoutDirection();

        if (!gtk && 1==text.count()) {
            QStyledItemDelegate::paint(painter, option, index);
        } else {
            bool selected=option.state&QStyle::State_Selected;
            bool active=option.state&QStyle::State_Active;
            if ((option.state&QStyle::State_MouseOver) && gtk) {
                GtkStyle::drawSelection(option, painter, selected ? 0.75 : 0.25);
            } else {
                QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);
            }
            QRect r(option.rect);
            r.adjust(4, 0, -4, 0);
            QPixmap pix=index.data(Qt::DecorationRole).value<QIcon>().pixmap(treeDecorationSize, treeDecorationSize);
            if (gtk && pix.isNull()) {
                QVariant image = index.data(ItemView::Role_Image);
                if (!image.isNull()) {
                    pix = QVariant::Pixmap==image.type() ? image.value<QPixmap>() : image.value<QIcon>().pixmap(listDecorationSize, listDecorationSize);
                }
            }
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
                QColor color(option.palette.color(active ? QPalette::Active : QPalette::Inactive, selected ? QPalette::HighlightedText : QPalette::Text));
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

        if ((option.state & QStyle::State_MouseOver)) {
            drawIcons(painter, option.rect, true, rtl, AP_HMiddle, index);
        }
    }

    void setSimple(bool s) { simpleStyle=s; }

    bool simpleStyle;
};

ItemView::Mode ItemView::toMode(const QString &str)
{
    for (int i=0; i<Mode_Count; ++i) {
        if (modeStr((Mode)i)==str) {
            return (Mode)i;
        }
    }

    // Older versions of Cantata saved mode as an integer!!!
    int i=str.toInt();
    switch (i) {
        default:
        case 0: return Mode_SimpleTree;
        case 1: return Mode_List;
        case 2: return Mode_IconTop;
        case 3: return Mode_GroupedTree;
    }
}

QString ItemView::modeStr(Mode m)
{
    switch (m) {
    default:
    case Mode_SimpleTree:   return QLatin1String("simpletree");
    case Mode_DetailedTree: return QLatin1String("detailedtree");
    case Mode_List:         return QLatin1String("list");
    case Mode_IconTop:      return QLatin1String("icontop");
    case Mode_GroupedTree:  return QLatin1String("grouped");
    }
}

void ItemView::setForceSingleClick(bool v)
{
    forceSingleClick=v;
}

bool ItemView::getForceSingleClick()
{
    return forceSingleClick;
}

ItemView::ItemView(QWidget *p)
    : QWidget(p)
    , searchTimer(0)
    , itemModel(0)
    , currentLevel(0)
    , mode(Mode_SimpleTree)
    , groupedView(0)
    , spinner(0)
{
    setupUi(this);
    backAction = new QAction(i18n("Back"), this);
    backAction->setIcon(Icon("go-previous"));
    backButton->setDefaultAction(backAction);
    //backButton->setMaximumHeight(listSearch->sizeHint().height());
    backAction->setShortcut(Qt::Key_Escape);
    listView->addAction(backAction);
    listView->addDefaultAction(backAction);
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    listView->addAction(sep);
    Icon::init(backButton);
    treeView->setPageDefaults();
    iconGridSize=listGridSize=listView->gridSize();
    listView->installEventFilter(new ListViewEventHandler(listView, backAction));
    listView->setItemDelegate(new ListDelegate(listView, listView));
    treeView->setItemDelegate(new TreeDelegate(treeView));
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
}

ItemView::~ItemView()
{
}

void ItemView::allowGroupedView()
{
    if (!groupedView) {
        groupedView=new GroupedView(stackedWidget);
        groupedView->setAutoExpand(false);
        groupedView->setMultiLevel(true);
        treeLayout->addWidget(groupedView);
        connect(groupedView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
        if (SINGLE_CLICK) {
            connect(groupedView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
        }
        connect(groupedView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
        connect(groupedView, SIGNAL(clicked(const QModelIndex &)), this, SLOT(itemClicked(const QModelIndex &)));
    }
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
        m=Mode_SimpleTree;
    }

    if (m==mode) {
        return;
    }

    mode=m;
    treeSearch->setText(QString());
    listSearch->setText(QString());
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
        treeView->setModel(itemModel);
        listView->setModel(0);
        if (groupedView) {
            groupedView->setHidden(true);
            groupedView->setModel(0);
        }
        treeView->setHidden(false);
        ((TreeDelegate *)treeView->itemDelegate())->setSimple(Mode_SimpleTree==mode);
        itemModel->setRootIndex(QModelIndex());
        treeView->reset();
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
        emit rootIndexSet(QModelIndex());
        if (Mode_IconTop!=mode) {
            listView->setGridSize(listGridSize);
            listView->setViewMode(QListView::ListMode);
            listView->setResizeMode(QListView::Fixed);
            listView->setAlternatingRowColors(true);
            listView->setWordWrap(false);
        }
    }

    stackedWidget->setCurrentIndex(Mode_SimpleTree==mode || Mode_DetailedTree==mode || Mode_GroupedTree==mode ? 0 : 1);
    if (spinner) {
        spinner->setWidget(view()->viewport());
        if (spinner->isActive()) {
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
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
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
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
        return treeView;
    } else if(Mode_GroupedTree==mode) {
        return groupedView;
    } else {
        return listView;
    }
}

void ItemView::setModel(ProxyModel *m)
{
    bool needtToInit=!itemModel;
    itemModel=m;
    if (needtToInit) {
        mode=Mode_List;
        setMode(Mode_SimpleTree);
    }
    view()->setModel(m);
}

QString ItemView::searchText() const
{
    return Mode_SimpleTree==mode || Mode_DetailedTree==mode || Mode_GroupedTree==mode ? treeSearch->text() : listSearch->text();
}

void ItemView::clearSearchText()
{
    return Mode_SimpleTree==mode || Mode_DetailedTree==mode || Mode_GroupedTree==mode ? treeSearch->setText(QString()) : listSearch->setText(QString());
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
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
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
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
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
            QModelIndex p=idx;
            while (i.isValid()) {
                indexes.prepend(i);
                i=i.parent();
            }
            setLevel(0);
            listView->setRootIndex(QModelIndex());
            itemModel->setRootIndex(QModelIndex());
            foreach (const QModelIndex &i, indexes) {
                activateItem(i, false);
            }
            if (p.isValid()) {
                emit rootIndexSet(p);
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
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode || Mode_GroupedTree==mode) {
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
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
        treeView->expandAll();
    } else if (Mode_GroupedTree==mode && groupedView) {
        groupedView->expandAll();
    }
}

void ItemView::showSpinner()
{
    if (!spinner) {
        spinner=new Spinner(this);
    }
    spinner->setWidget(view()->viewport());
    spinner->start();
}

void ItemView::hideSpinner()
{
    if (spinner) {
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
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode || Mode_GroupedTree==mode || 0==currentLevel) {
        return;
    }
    setLevel(currentLevel-1);
    itemModel->setRootIndex(listView->rootIndex().parent());
    listView->setRootIndex(listView->rootIndex().parent());
    emit rootIndexSet(listView->rootIndex().parent());

    if (qobject_cast<QSortFilterProxyModel *>(listView->model())) {
        QModelIndex idx=static_cast<QSortFilterProxyModel *>(listView->model())->mapFromSource(prevTopIndex);
        if (idx.isValid()) {
            listView->scrollTo(idx, QAbstractItemView::PositionAtTop);
        }
    } else {
        listView->scrollTo(prevTopIndex, QAbstractItemView::PositionAtTop);
    }
}

void ItemView::setExpanded(const QModelIndex &idx, bool exp)
{
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
        treeView->setExpanded(idx, exp);
    }
}

QAction * ItemView::getAction(const QModelIndex &index)
{
    QAbstractItemDelegate *abs=view()->itemDelegate();
    ActionItemDelegate *d=abs ? qobject_cast<ActionItemDelegate *>(abs) : 0;
    return d ? d->getAction(index) : 0;
}

void ItemView::itemClicked(const QModelIndex &index)
{
    QAction *act=getAction(index);
    if (act) {
        act->trigger();
        return;
    }

    if (forceSingleClick) {
        activateItem(index);
    }
}

void ItemView::itemActivated(const QModelIndex &index)
{
    if (!forceSingleClick) {
        activateItem(index);
    }
}

void ItemView::activateItem(const QModelIndex &index, bool emitRootSet)
{
    if (getAction(index)) {
        return;
    }

    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
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
        if (emitRootSet) {
            emit rootIndexSet(index);
        }
        listView->scrollToTop();
    }
}

void ItemView::delaySearchItems()
{
    bool isTree=Mode_SimpleTree==mode || Mode_DetailedTree==mode || Mode_GroupedTree==mode;
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
