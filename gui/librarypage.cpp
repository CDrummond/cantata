/*
 * Cantata
 *
 * Copyright (c) 2011 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "librarypage.h"
#include "mpdconnection.h"
#include "mpdparseutils.h"
#include "covers.h"
#include "musiclibraryitemalbum.h"
#include "musiclibraryitemsong.h"
#include "mainwindow.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QStyle>
#include <QtGui/QStyleOptionViewItem>
#include <QtGui/QPainter>
#include <QtCore/QTimer>
#ifdef ENABLE_KDE_SUPPORT
#include <KDE/KAction>
#include <KDE/KLocale>
#include <KDE/KActionCollection>
#include <KDE/KGlobalSettings>
#else
#include <QtGui/QAction>
#endif

class LibraryItemDelegate : public QStyledItemDelegate
{
public:
    static const int constBorder = 1;
    static const int constActionBorder = 2;
    static const int constActionIconSize=16;

    LibraryItemDelegate(QObject *p, MusicLibraryProxyModel *pr, Action *aa, Action *ra)
        : QStyledItemDelegate(p)
        , proxy(pr)
        , addAction(aa)
        , replaceAction(ra)
    {
    }

    virtual ~LibraryItemDelegate()
    {
    }

    int imgSize(MusicLibraryItem *item) const
    {
       return item && item->type()!=MusicLibraryItem::Type_Album
            ? qMin(MusicLibraryItemAlbum::coverPixels(), 22)
            : MusicLibraryItemAlbum::coverPixels();
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy->mapToSource(index).internalPointer());
        QSize sz(QStyledItemDelegate::sizeHint(option, index));
        int textHeight = QApplication::fontMetrics().height()*2;
        int imageSize=imgSize(item);

        return QSize(qMax(sz.width(), imageSize) + (constBorder * 2),
                     qMax(qMax(textHeight, sz.height()), imageSize)  + (constBorder*2));
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);

        MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy->mapToSource(index).internalPointer());
        QString text = index.data(Qt::DisplayRole).toString();
        QPixmap pix;
        QRect r(option.rect);
        QString childText;
        int imageSize=imgSize(item);

        if (item) {
            switch (item->type()) {
            case MusicLibraryItem::Type_Artist:
                pix=QIcon::fromTheme("view-media-artist").pixmap(imageSize, imageSize);
                #ifdef ENABLE_KDE_SUPPORT
                childText=i18np("1 Album", "%1 Albums", item->childCount());
                #else
                childText=1==item->childCount() ? tr("1 Album") : tr("%1 Albums").arg(item->childCount());
                #endif
                break;
            case MusicLibraryItem::Type_Song:
                pix=QIcon::fromTheme("audio-x-generic").pixmap(imageSize, imageSize);
                childText=MPDParseUtils::formatDuration(static_cast<MusicLibraryItemSong *>(item)->time());
                if (childText.startsWith(QLatin1String("00:"))) {
                    childText=childText.mid(3);
                }
                if (childText.startsWith(QLatin1String("00:"))) {
                    childText=childText.mid(1);
                }
                break;
            case MusicLibraryItem::Type_Album:
                pix=static_cast<MusicLibraryItemAlbum *>(item)->cover();
                #ifdef ENABLE_KDE_SUPPORT
                childText=i18np("1 Song", "%1 Songs", item->childCount());
                #else
                childText=1==item->childCount() ? tr("1 Song") : tr("%1 Songs").arg(item->childCount());
                #endif
                break;
            default:
                break;
            }
        }

        r.adjust(constBorder, 0, -constBorder, 0);
        if (!pix.isNull()) {
            painter->drawPixmap(r.x(), r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
            r.adjust(pix.width()+3, 0, -3, 0);
        }
        QFont textFont(QApplication::font());
        QFont childFont(QApplication::font());
        QFontMetrics textMetrics(textFont);
        childFont.setPointSizeF(childFont.pointSizeF()*0.8);
        QFontMetrics childMetrics(childFont);
        bool selected=option.state&QStyle::State_Selected;
        QColor color(QApplication::palette().color(selected ? QPalette::HighlightedText : QPalette::Text));
        QRect textRect;
        QRect childRect;

        int textHeight=textMetrics.height();
        int childHeight=childMetrics.height();
        int totalHeight=textHeight+childHeight;
        textRect=QRect(r.x(), r.y()+((r.height()-totalHeight)/2), r.width(), textHeight);
        childRect=QRect(r.x(), r.y()+textHeight+((r.height()-totalHeight)/2), r.width(), textHeight);
        text = textMetrics.elidedText(text, Qt::ElideRight, textRect.width(), QPalette::WindowText);

        painter->setPen(color);
        painter->setFont(textFont);
        painter->drawText(textRect, text);

        childText = childMetrics.elidedText(childText, Qt::ElideRight, childRect.width(), QPalette::WindowText);
        color.setAlphaF(selected ? 0.65 : 0.5);
        painter->setPen(color);
        painter->setFont(childFont);
        painter->drawText(childRect, childText);

        if (option.state & QStyle::State_MouseOver) {
            if (r.width()>(constActionIconSize+(2*constActionBorder))) {
                pix=replaceAction->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
                if (!pix.isNull()) {
                    painter->drawPixmap(r.x()+r.width()-(pix.width()+constActionBorder),
                                        r.y()+((r.height()-pix.height())/2),
                                        pix.width(), pix.height(), pix);
                    r.adjust(0, 0, -(pix.width()+constActionBorder), 0);
                }
            }

            if (r.width()>(constActionIconSize+(2*constActionBorder))) {
                pix=addAction->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
                if (!pix.isNull()) {
                    painter->drawPixmap(r.x()+r.width()-(pix.width()+constActionBorder),
                                        r.y()+((r.height()-pix.height())/2),
                                        pix.width(), pix.height(), pix);
                }
            }
        }
    }

    MusicLibraryProxyModel *proxy;
    Action *addAction;
    Action *replaceAction;
};

LibraryPage::LibraryPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    #ifdef ENABLE_KDE_SUPPORT
    backAction = p->actionCollection()->addAction("back");
    backAction->setText(i18n("Back"));
    homeAction = p->actionCollection()->addAction("home");
    homeAction->setText(i18n("Artists"));
    #else
    backAction = new QAction(tr("Back"), this);
    homeAction = new QAction(tr("Artists"), this);
    #endif
    addToPlaylistAction=p->addToPlaylistAction;
    replacePlaylistAction=p->replacePlaylistAction;
    backAction->setIcon(QIcon::fromTheme("go-previous"));
    homeAction->setIcon(QIcon::fromTheme("view-media-artist"));
    addToPlaylist->setDefaultAction(addToPlaylistAction);
    replacePlaylist->setDefaultAction(replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->updateDbAction);
    backButton->setDefaultAction(backAction);
    homeButton->setDefaultAction(homeAction);

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);
    backButton->setAutoRaise(true);
    homeButton->setAutoRaise(true);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);

    treeView->setPageDefaults();
    treeView->addAction(addToPlaylistAction);
    treeView->addAction(replacePlaylistAction);
    listView->addAction(addToPlaylistAction);
    listView->addAction(replacePlaylistAction);
    listView->addAction(backAction);
    listView->addAction(homeAction);
    listView->setItemDelegate(new LibraryItemDelegate(this, &proxy, addToPlaylistAction, replacePlaylistAction));
    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(searchTree, SIGNAL(returnPressed()), this, SLOT(searchItems()));
    connect(searchTree, SIGNAL(textChanged(const QString)), this, SLOT(searchItems()));
    connect(searchList, SIGNAL(returnPressed()), this, SLOT(searchItems()));
    connect(searchList, SIGNAL(textChanged(const QString)), this, SLOT(searchItems()));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(MPDConnection::self(), SIGNAL(musicLibraryUpdated(MusicLibraryItemRoot *, QDateTime)), &model, SLOT(updateMusicLibrary(MusicLibraryItemRoot *, QDateTime)));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &)),
            &model, SLOT(setCover(const QString &, const QString &, const QImage &)));
    connect(&model, SIGNAL(updateGenres(const QStringList &)), this, SLOT(updateGenres(const QStringList &)));
    connect(this, SIGNAL(listAllInfo(const QDateTime &)), MPDConnection::self(), SLOT(listAllInfo(const QDateTime &)));
    connect(treeView, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(treeView, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));
    connect(treeView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(treeView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(listView, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(listView, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));
    connect(listView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
    connect(listView, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
    connect(backAction, SIGNAL(triggered(bool)), this, SLOT(backActivated()));
    connect(homeAction, SIGNAL(triggered(bool)), this, SLOT(homeActivated()));
    proxy.setSourceModel(&model);
#ifdef ENABLE_KDE_SUPPORT
    searchTree->setPlaceholderText(i18n("Search library..."));
#else
    searchTree->setPlaceholderText(tr("Search library..."));
#endif
    usingTreeView=false;
    setView(true);
}

LibraryPage::~LibraryPage()
{
}

void LibraryPage::setView(bool tree)
{
    if (tree==usingTreeView) {
        return;
    }

    usingTreeView=tree;
    searchTree->setText(QString());
    searchList->setText(QString());
    if (tree) {
        treeView->setModel(&proxy);
        listView->setModel(0);
    } else {
        homeActivated();
        treeView->setModel(0);
        listView->setModel(&proxy);
    }
    stackedWidget->setCurrentIndex(tree ? 0 : 1);
}

void LibraryPage::setLevel(int level, const MusicLibraryItem *i)
{
//     int cLevel=currentLevel;
    currentLevel=level;

    backAction->setEnabled(0!=level);
    homeAction->setEnabled(0!=level);
//     if (level>cLevel) {
//         prevSearch=searchList->text();
//         searchList->setText(QString());
//     } else {
//         searchList->setText(prevSearch);
//         prevSearch=QString();
//     }
#ifdef ENABLE_KDE_SUPPORT
    switch(level) {
    default:
    case 0:
        searchList->setPlaceholderText(i18n("Search library..."));
        break;
    case 1:
        if (i) {
            searchList->setPlaceholderText(i18n("Search %1 albums...", i->data(0).toString()));
        } else {
            searchList->setPlaceholderText(i18n("Search albums..."));
        }
        break;
    case 2:
        if (i) {
            searchList->setPlaceholderText(i18n("Search %1 songs...", i->data(0).toString()));
        } else {
            searchList->setPlaceholderText(i18n("Search songs..."));
        }
        break;
    }
#else
    switch(level) {
    default:
    case 0:
        searchList->setPlaceholderText(tr("Search library..."));
        break;
    case 1:
        if (i) {
            searchList->setPlaceholderText(tr("Search %1 albums...", i->data(0).toString()));
        } else {
            searchList->setPlaceholderText(tr("Search albums..."));
        }
        break;
    case 2:
        if (i) {
            searchList->setPlaceholderText(tr("Search %1 songs...", i->data(0).toString()));
        } else {
            searchList->setPlaceholderText(tr("Search songs..."));
        }
        break;
    }
#endif
    view()->selectionModel()->clearSelection();
}

QAbstractItemView * LibraryPage::view()
{
    return usingTreeView ? (QAbstractItemView *)treeView : (QAbstractItemView *)listView;
}

void LibraryPage::refresh(Refresh type)
{
    homeActivated();
    if (RefreshForce==type) {
        model.clearUpdateTime();
    }

    if (RefreshFromCache!=type || !model.fromXML(MPDStats::self()->dbUpdate())) {
        emit listAllInfo(MPDStats::self()->dbUpdate());
    }
}

void LibraryPage::clear()
{
    model.clear();
    homeActivated();
}

void LibraryPage::addSelectionToPlaylist()
{
    QStringList files;
    MusicLibraryItem *item;
    MusicLibraryItemSong *songItem;

    // Get selected indexes
    const QModelIndexList selected = view()->selectionModel()->selectedIndexes();
    int selectionSize = selected.size();

    if (selectionSize == 0) {
        return;
    }

    // Loop over the selection. Only add files.
    for (int selectionPos = 0; selectionPos < selectionSize; selectionPos++) {
        const QModelIndex current = selected.at(selectionPos);
        item = static_cast<MusicLibraryItem *>(proxy.mapToSource(current).internalPointer());

        switch (item->type()) {
        case MusicLibraryItem::Type_Artist: {
            for (quint32 i = 0; ; i++) {
                const QModelIndex album = current.child(i , 0);
                if (!album.isValid())
                    break;

                for (quint32 j = 0; ; j++) {
                    const QModelIndex track = album.child(j, 0);
                    if (!track.isValid())
                        break;
                    const QModelIndex mappedSongIndex = proxy.mapToSource(track);
                    songItem = static_cast<MusicLibraryItemSong *>(mappedSongIndex.internalPointer());
                    const QString fileName = songItem->file();
                    if (!fileName.isEmpty() && !files.contains(fileName))
                        files.append(fileName);
                }
            }
            break;
        }
        case MusicLibraryItem::Type_Album: {
            for (quint32 i = 0; ; i++) {
                QModelIndex track = current.child(i, 0);
                if (!track.isValid())
                    break;
                const QModelIndex mappedSongIndex = proxy.mapToSource(track);
                songItem = static_cast<MusicLibraryItemSong *>(mappedSongIndex.internalPointer());
                const QString fileName = songItem->file();
                if (!fileName.isEmpty() && !files.contains(fileName))
                    files.append(fileName);
            }
            break;
        }
        case MusicLibraryItem::Type_Song: {
            const QString fileName = static_cast<MusicLibraryItemSong *>(item)->file();
            if (!fileName.isEmpty() && !files.contains(fileName))
                files.append(fileName);
            break;
        }
        default:
            break;
        }
    }

    if (!files.isEmpty()) {
        emit add(files);
        view()->selectionModel()->clearSelection();
    }
}

void LibraryPage::homeActivated()
{
    if (usingTreeView) {
        return;
    }

    setLevel(0);
    listView->setRootIndex(QModelIndex());
//     prevSearch=QString();
//     searchList->setText(prevSearch);
}

void LibraryPage::backActivated()
{
    if (usingTreeView) {
        return;
    }
    setLevel(currentLevel-1, static_cast<MusicLibraryItem *>(proxy.mapToSource(listView->rootIndex().parent()).internalPointer()));
    listView->setRootIndex(listView->rootIndex().parent());
    listView->scrollTo(prevTopIndex, QAbstractItemView::PositionAtTop);
}

void LibraryPage::itemClicked(const QModelIndex &index)
{
    QRect rect(listView->visualRect(index));
    rect.moveTo(listView->viewport()->mapToGlobal(QPoint(rect.x(), rect.y())));
    QRect actionRect(rect.x()+rect.width()-(LibraryItemDelegate::constActionIconSize+LibraryItemDelegate::constActionBorder),
                     rect.y()+((rect.height()-LibraryItemDelegate::constActionIconSize)/2),
                     LibraryItemDelegate::constActionIconSize, LibraryItemDelegate::constActionIconSize);

    if (actionRect.contains(QCursor::pos())) {
        replacePlaylistAction->trigger();
        return;
    }
    actionRect.adjust(-(LibraryItemDelegate::constActionIconSize+LibraryItemDelegate::constActionBorder), 0, 0, 0);
    if (actionRect.contains(QCursor::pos())) {
        addToPlaylistAction->trigger();
        return;
    }

#ifdef ENABLE_KDE_SUPPORT
    if (KGlobalSettings::singleClick()) {
        itemActivated(index);
    }
#endif
}

void LibraryPage::itemActivated(const QModelIndex &index)
{
    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy.mapToSource(index).internalPointer());

    if (MusicLibraryItem::Type_Song!=item->type()) {
        if (usingTreeView) {
            treeView->setExpanded(index, !treeView->isExpanded(index));
        } else {
            prevTopIndex=listView->indexAt(QPoint(0, 0));
            setLevel(currentLevel+1, item);
            listView->setRootIndex(index);
            listView->scrollToTop();
        }
    }
}

void LibraryPage::itemDoubleClicked(const QModelIndex &)
{
    const QModelIndexList selected = view()->selectionModel()->selectedIndexes();
    const QModelIndex current = selected.at(0);
    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy.mapToSource(current).internalPointer());
    if (MusicLibraryItem::Type_Song==item->type()) {
        addSelectionToPlaylist();
    }
}

void LibraryPage::searchItems()
{
    QString text=usingTreeView ? searchTree->text() : searchList->text();
    proxy.setFilterGenre(0==genreCombo->currentIndex() ? QString() : genreCombo->currentText());
//     proxy.setFilterField(usingTreeView ? 3 : currentLevel);
    proxy.setFilterRegExp(text);
}

void LibraryPage::updateGenres(const QStringList &genres)
{
    QStringList entries;
#ifdef ENABLE_KDE_SUPPORT
    entries << i18n("All Genres");
#else
    entries << tr("All Genres");
#endif
    entries+=genres;

    bool diff=genreCombo->count() != entries.count();
    if (!diff) {
        // Check items...
        for (int i=1; i<genreCombo->count() && !diff; ++i) {
            if (genreCombo->itemText(i) != entries.at(i)) {
                diff=true;
            }
        }
    }

    if (!diff) {
        return;
    }

    QString currentFilter = genreCombo->currentIndex() ? genreCombo->currentText() : QString();

    genreCombo->clear();
    if (genres.count()<2) {
        genreCombo->setCurrentIndex(0);
    } else {
        genreCombo->addItems(entries);
        if (!currentFilter.isEmpty()) {
            bool found=false;
            for (int i=1; i<genreCombo->count() && !found; ++i) {
                if (genreCombo->itemText(i) == currentFilter) {
                    genreCombo->setCurrentIndex(i);
                    found=true;
                }
            }
            if (!found) {
                genreCombo->setCurrentIndex(0);
            }
        }
    }
}
