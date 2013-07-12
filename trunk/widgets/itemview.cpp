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
#include "messageoverlay.h"
#include <QToolButton>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QPainter>
#include <QAction>
#include <QTimer>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KGlobalSettings>
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

ViewEventHandler::ViewEventHandler(ActionItemDelegate *d, QAbstractItemView *v)
    : QObject(v)
    , delegate(d)
    , view(v)
{
}

// HACK time. For some reason, IconView is not always re-drawn when mouse leaves the view.
// We sometimes get an item that is left in the mouse-over state. So, work-around this by
// keeping track of when mouse is over listview.
bool ViewEventHandler::eventFilter(QObject *obj, QEvent *event)
{
    if (delegate) {
        if (QEvent::Enter==event->type()) {
            delegate->setUnderMouse(true);
            view->viewport()->update();
        } else if (QEvent::Leave==event->type()) {
            delegate->setUnderMouse(false);
            view->viewport()->update();
        }
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

        if (!underMouse) {
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

        if (childText==QLatin1String("-")) {
            childText.clear();
        }

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
                QRect border(xpos, r.y(), pix.width(), pix.height());
                QRect shadow(border);
                for (int i=0; i<3; ++i) {
                    shadow.adjust(1, 1, 1, 1);
                    color.setAlphaF(alphas[i]);
                    painter->setPen(color);
                    painter->drawLine(shadow.bottomLeft()+QPoint(i+1, 0),
                                      shadow.bottomRight()+QPoint(-((i*2)+2), 0));
                    painter->drawLine(shadow.bottomRight()+QPoint(0, -((i*2)+2)),
                                      shadow.topRight()+QPoint(0, i+1));
                    if (1==i) {
                        painter->drawPoint(shadow.bottomRight()-QPoint(2, 1));
                        painter->drawPoint(shadow.bottomRight()-QPoint(1, 2));
                        painter->drawPoint(shadow.bottomLeft()-QPoint(1, 1));
                        painter->drawPoint(shadow.topRight()-QPoint(1, 1));
                    } else if (2==i) {
                        painter->drawPoint(shadow.bottomRight()-QPoint(4, 1));
                        painter->drawPoint(shadow.bottomRight()-QPoint(1, 4));
                        painter->drawPoint(shadow.bottomLeft()-QPoint(0, 1));
                        painter->drawPoint(shadow.topRight()-QPoint(1, 0));
                        painter->drawPoint(shadow.bottomRight()-QPoint(2, 2));
                    }
                }
                color.setAlphaF(0.4);
                painter->setPen(color);
                painter->drawRect(border.adjusted(0, 0, -1, -1));
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

//        if (!mouseOver) {
//            drawIcons(painter, AP_VTop==actionPos ? r2 : r, false, rtl, actionPos, index);
//        }

        QRect textRect;
        QColor color(option.palette.color(active ? QPalette::Active : QPalette::Inactive, selected ? QPalette::HighlightedText : QPalette::Text));
        QTextOption textOpt(AP_VTop==actionPos ? Qt::AlignHCenter|Qt::AlignVCenter : Qt::AlignVCenter);

        textOpt.setWrapMode(QTextOption::NoWrap);
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

            if (!childText.isEmpty()) {
                childText = childMetrics.elidedText(childText, Qt::ElideRight, childRect.width(), QPalette::WindowText);
                color.setAlphaF(subTextAlpha(selected));
                painter->setPen(color);
                painter->setFont(childFont);
                painter->drawText(childRect, childText, textOpt);
            }
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
        drawDivider(painter, option.rect, color);
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
        bool selected=option.state&QStyle::State_Selected;
        bool active=option.state&QStyle::State_Active;
        if (!gtk && 1==text.count()) {
            QStyledItemDelegate::paint(painter, option, index);
        } else {
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
        drawDivider(painter, option.rect, option.palette.color(active ? QPalette::Active : QPalette::Inactive,
                                                               selected ? QPalette::HighlightedText : QPalette::Text));
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

ItemView::ItemView(QWidget *p)
    : QWidget(p)
    , searchTimer(0)
    , itemModel(0)
    , currentLevel(0)
    , mode(Mode_SimpleTree)
    , groupedView(0)
    , spinner(0)
    , msgOverlay(0)
{
    setupUi(this);
    backAction = new QAction(i18n("Go Back"), this);
    backAction->setIcon(Icon("go-previous"));
    backButton->setDefaultAction(backAction);
    backAction->setShortcut(Qt::AltModifier+(Qt::LeftToRight==layoutDirection() ? Qt::Key_Left : Qt::Key_Right));
    homeAction = new QAction(i18n("Go Home"), this);
    homeAction->setIcon(Icon("go-home"));
    homeButton->setDefaultAction(homeAction);
    homeAction->setShortcut(Qt::AltModifier+Qt::Key_Up);
    listView->addAction(backAction);
    listView->addAction(homeAction);
    listView->addDefaultAction(backAction);
    listView->addDefaultAction(homeAction);
    QAction *sep=new QAction(this);
    sep->setSeparator(true);
    listView->addAction(sep);
    Icon::init(backButton);
    treeView->setPageDefaults();
    // Some styles, eg Cleanlooks/Plastique require that we explicitly set mouse tracking on the treeview.
    treeView->setAttribute(Qt::WA_MouseTracking);
    iconGridSize=listGridSize=listView->gridSize();
    ListDelegate *ld=new ListDelegate(listView, listView);
    TreeDelegate *td=new TreeDelegate(treeView);
    listView->setItemDelegate(ld);
    treeView->setItemDelegate(td);
    listView->installEventFilter(new ViewEventHandler(ld, listView));
    treeView->installEventFilter(new ViewEventHandler(td, treeView));
    connect(searchWidget, SIGNAL(returnPressed()), this, SLOT(delaySearchItems()));
    connect(searchWidget, SIGNAL(textChanged(const QString)), this, SLOT(delaySearchItems()));
    connect(searchWidget, SIGNAL(active(bool)), this, SIGNAL(searchIsActive(bool)));
    connect(treeView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
    connect(treeView, SIGNAL(itemActivated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
    connect(treeView, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
    connect(listView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
    connect(listView, SIGNAL(activated(const QModelIndex &)), this, SLOT(activateItem(const QModelIndex &)));
    connect(listView, SIGNAL(doubleClicked(const QModelIndex &)), this, SIGNAL(doubleClicked(const QModelIndex &)));
    connect(listView, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
    connect(backAction, SIGNAL(triggered(bool)), this, SLOT(backActivated()));
    connect(homeAction, SIGNAL(triggered(bool)), this, SLOT(homeActivated()));
    searchWidget->setVisible(false);
}

ItemView::~ItemView()
{
}

void ItemView::allowGroupedView()
{
    if (!groupedView) {
        groupedView=new GroupedView(stackedWidget);
        // Some styles, eg Cleanlooks/Plastique require that we explicitly set mouse tracking on the treeview.
        groupedView->setAttribute(Qt::WA_MouseTracking, true);
        groupedView->installEventFilter(new ViewEventHandler(qobject_cast<ActionItemDelegate *>(groupedView->itemDelegate()), groupedView));
        groupedView->setAutoExpand(false);
        groupedView->setMultiLevel(true);
        treeLayout->addWidget(groupedView);
        connect(groupedView, SIGNAL(itemsSelected(bool)), this, SIGNAL(itemsSelected(bool)));
        connect(groupedView, SIGNAL(itemActivated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
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

    QIcon oldBgndIcon=bgndIcon;
    if (!bgndIcon.isNull()) {
        setBackgroundImage(QIcon());
    }

    mode=m;
    searchWidget->setText(QString());
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
//            listView->setAlternatingRowColors(true);
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
    if (msgOverlay) {
        msgOverlay->setWidget(view()->viewport());
    }

    if (!oldBgndIcon.isNull()) {
        setBackgroundImage(oldBgndIcon);
    }
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
    homeAction->setEnabled(l>1);
    homeAction->setVisible(l>1);

    if (Mode_IconTop==mode) {
        if (0==currentLevel || haveChildren) {
            if (QListView::IconMode!=listView->viewMode()) {
                listView->setGridSize(iconGridSize);
                listView->setViewMode(QListView::IconMode);
                listView->setResizeMode(QListView::Adjust);
//                listView->setAlternatingRowColors(false);
                listView->setWordWrap(true);
                listView->setDragDropMode(QAbstractItemView::DragOnly);
                ((ActionItemDelegate *)listView->itemDelegate())->setLargeIcons(iconGridSize.width()>(ActionItemDelegate::constLargeActionIconSize*6));
            }
        } else if(QListView::ListMode!=listView->viewMode()) {
            listView->setGridSize(listGridSize);
            listView->setViewMode(QListView::ListMode);
            listView->setResizeMode(QListView::Fixed);
//            listView->setAlternatingRowColors(true);
            listView->setWordWrap(false);
            listView->setDragDropMode(QAbstractItemView::DragOnly);
            ((ActionItemDelegate *)listView->itemDelegate())->setLargeIcons(false);
        }
    }

    if (view()->selectionModel()) {
        view()->selectionModel()->clearSelection();
    }

    backButton->setVisible(currentLevel>0);
    title->setVisible(currentLevel>0);
    homeButton->setVisible(currentLevel>1);

    if (currentLevel>0) {
        if (prev>currentLevel) {
            title->setText(prevText[currentLevel]);
        } else {
            prevText.insert(prev, title->text());
        }
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
    return searchWidget->isVisible() ? searchWidget->text() : QString();
}

void ItemView::clearSearchText()
{
    return searchWidget->setText(QString());
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
        ((ActionItemDelegate *)listView->itemDelegate())->setLargeIcons(iconGridSize.width()>(ActionItemDelegate::constLargeActionIconSize*6));
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
    searchWidget->toggle();
}

void ItemView::setSearchLabelText(const QString &text)
{
    searchWidget->setLabel(text);
}

void ItemView::setSearchVisible(bool v)
{
    searchWidget->setVisible(v);
}

bool ItemView::isSearchActive() const
{
    return searchWidget->isActive();
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

void ItemView::expand(const QModelIndex &index)
{
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
        treeView->expand(index);
    } else if (Mode_GroupedTree==mode && groupedView) {
        groupedView->expand(index);
    }
}

void ItemView::showMessage(const QString &message, int timeout)
{
    if (!msgOverlay) {
        msgOverlay=new MessageOverlay(this);
        msgOverlay->setWidget(view()->viewport());
    }
    msgOverlay->setText(message, timeout, timeout<=0);
}

void ItemView::setBackgroundImage(const QIcon &icon)
{
    bgndIcon=icon;
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode) {
        treeView->setBackgroundImage(bgndIcon);
    } else if (Mode_GroupedTree==mode && groupedView) {
        groupedView->setBackgroundImage(bgndIcon);
    } else if (Mode_List==mode || Mode_IconTop==mode) {
        listView->setBackgroundImage(bgndIcon);
    }
}

void ItemView::showSpinner(bool v)
{
    if (v) {
        if (!spinner) {
            spinner=new Spinner(this);
        }
        spinner->setWidget(view()->viewport());
        spinner->start();
    } else {
        hideSpinner();
    }
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

void ItemView::homeActivated()
{
    if (Mode_SimpleTree==mode || Mode_DetailedTree==mode || Mode_GroupedTree==mode || 0==currentLevel) {
        return;
    }
    setLevel(0);
    itemModel->setRootIndex(QModelIndex());
    listView->setRootIndex(QModelIndex());
    emit rootIndexSet(QModelIndex());
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

    if (TreeView::getForceSingleClick()) {
        activateItem(index);
    }
}

void ItemView::itemActivated(const QModelIndex &index)
{
    if (!TreeView::getForceSingleClick()) {
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
    } else if (index.isValid() && index.child(0, 0).isValid() && index!=listView->rootIndex()) {
        prevTopIndex=listView->indexAt(QPoint(8, 8));
        if (qobject_cast<QSortFilterProxyModel *>(listView->model())) {
            prevTopIndex=static_cast<QSortFilterProxyModel *>(listView->model())->mapToSource(prevTopIndex);
        }
        setLevel(currentLevel+1, index.child(0, 0).child(0, 0).isValid());
        QString text=index.data(Role_TitleText).toString();
        if (text.isEmpty()) {
            text=index.data(Qt::DisplayRole).toString();
        }
        title->setText(text);
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
    if (searchWidget->text().isEmpty()) {
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
