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
#include "covers.h"
#include "musiclibraryitemsong.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QStyle>
#include <QtGui/QStyleOptionViewItem>
#ifdef ENABLE_KDE_SUPPORT
#include <KAction>
#include <KLocale>
#include <KActionCollection>
#else
#include <QAction>
#endif

// class LibraryItemDelegate : public QStyledItemDelegate
// {
//     static const int constBorder = 6;
//
// public:
//     LibraryItemDelegate(QObject *p)
//         : QStyledItemDelegate(p))
//     {
//     }
//
//     virtual ~LibraryItemDelegate()
//     {
//     }
//
//     QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
//     {
//         QSize sz(QStyledItemDelegate::sizeHint(option, index));
//         int textHeight = QApplication::fontMetrics().height()*2.2;
//
//         return QSize(qMax(sz.width(), MusicLibraryItemAlbum::coverPixels()) + (constBorder * 2),
//                      qMax((textHeight + constBorder) * 3, sz.height() + (constBorder * 2)));
//     }
//
//     void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
//     {
//         if (!index.isValid()) {
//             return;
//         }
//         QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);
//
//         MusicLibraryItem *item = static_cast<MusicLibraryItem *>(index.internalPointer());
//         QString text = index.data(Qt::DisplayRole).toText();
//         int children = index.data(Qt::UserRole).toInt();
//         QVariant decoration = index.data(Qt::DecorationRole).toText();
//         QPixmap pix;
//         QRect r(option.rect);
//         QString childType;
//
//         if (item) {
//             switch (item->type()) {
//             case MusicLibraryItem::Type_Artist:
//             case MusicLibraryItem::Type_Song:
//                 pix=decoration.value<QIcon>().pixmap(MusicLibraryItemAlbum::coverPixels(), MusicLibraryItemAlbum::coverPixels());
//                 break;
//             case MusicLibraryItem::Type_Album:
//                 pix=decoration.value<QPixmap>();
//                 break;
//             }
//         }
//
//         int x=r.x()+constBorder;
//         if (!pix.isNull()) {
//             painter->drawPixmap(x, r.y()+((r.height()-pix.height())/2), pix.width(), pix.height(), pix);
//             x+=pix.width()+constBorder;
//         }
//
// //         QFont f(QApplication::font());
// //         f.setBold(true);
// //         QFontMetrics fm(f);
// //         text = fm.elidedText(text, Qt::ElideRight, r.width()-(constBorder+(x-r.x())), QPalette::WindowText);
// //
// //         painter->setPen(txtCol);
// //         painter->setFont(f);
// //         painter->drawText(
//
//         QTextDocument doc;
//         doc.setFont(QApplication::font());
//         QString html=i18n("<b>%1</b></br><small>%2</small>").arg(text).arg(children)
//         doc.setHtml(user.toString());
//         if ((QVariant::Pixmap == dec.type()) && (QVariant::String == disp.type()) &&
//             (QVariant::String == user.type())) {
//             m_widget->style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option,
//                                                 painter, 0L);
//
//             QRect                r(option.rect);
//             QFont                fnt(m_widget->font());
//             QPalette::ColorGroup cg = option.state & QStyle::State_Enabled ?
//                                                         QPalette::Normal : QPalette::Disabled;
//             QPixmap              pix(dec.value<QPixmap>());
//             int                  textHeight = m_widget->fontMetrics().height();
//             int                  iconPosModX = constBorder + ((KMF::MediaObject::constIconSize -
//                                                                 pix.width()) / 2);
//             int                  iconPosModY = (option.rect.height() - pix.height()) / 2;
//
//             painter->drawPixmap(r.adjusted(iconPosModX, iconPosModY, iconPosModX,
//                                 iconPosModY).topLeft(), pix);
//
//             fnt.setBold(true);
//
//             painter->setFont(fnt);
//
//             if ((QPalette::Normal == cg) && !(option.state & QStyle::State_Active)) {
//                 cg = QPalette::Inactive;
//             }
//
//             r.adjust(KMF::MediaObject::constIconSize + (constBorder * 2),
//                         constBorder + textHeight, -constBorder, -constBorder);
//             QColor defColor = option.palette.color(cg, option.state &
//                     QStyle::State_Selected ? QPalette::HighlightedText : QPalette::Text);
//             painter->setPen(defColor);
//             painter->drawText(r.topLeft(), disp.toString());
//
//             // Draw info text. Can be rich text
//             r.adjust(0, constBorder, 0, 0);
//             QTextDocument doc;
//             doc.setDefaultFont(KGlobalSettings::smallestReadableFont());
//             doc.setDefaultStyleSheet(QString("body { color: %1; }").arg(defColor.name()));
//             doc.setHtml(user.toString());
//             painter->save();
//             painter->translate(r.topLeft());
//             doc.drawContents(painter, QRect(QPoint(0, 0), r.size()));
//             painter->restore();
//
//             if (option.state & QStyle::State_HasFocus) {
//                 QStyleOptionFocusRect o;
//                 o.QStyleOption::operator=(option);
//                 o.rect = option.rect;
//                 // m_widget->style()->subElementRect(QStyle::SE_ItemViewItemFocusRect,
//                 // &option, m_widget);
//                 o.state |= QStyle::State_KeyboardFocusChange;
//                 o.state |= QStyle::State_Item;
//                 cg = option.state &
//                         QStyle::State_Enabled  ? QPalette::Normal : QPalette::Disabled;
//                 o.backgroundColor = option.palette.color(
//                         cg, option.state &
//                         QStyle::State_Selected ? QPalette::Highlight : QPalette::Window);
//                 m_widget->style()->drawPrimitive(QStyle::PE_FrameFocusRect, &o, painter,
//                         m_widget);
//             }
//
//             if (option.state & QStyle::State_MouseOver &&
//                 kmfApp->project()->mediaObjects()->isValid(index)) {
//                 KMF::MediaObject *ob = kmfApp->project()->mediaObjects()->at(index);
//                 QList<QAction *> actions;
//
//                 ob->actions(&actions);
//                 r = option.rect;
//
//                 QSize                          size(constActionIconSize, constActionIconSize);
//                 QColor                         col(Qt::black);
//
//                 col.setAlphaF(0.35);
//
//             }
//         } else   {
//             QStyledItemDelegate::paint(painter, option, index);
//         }
//     }
// };

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
    backAction->setIcon(QIcon::fromTheme("go-previous"));
    homeAction->setIcon(QIcon::fromTheme("view-media-artist"));
    addToPlaylist->setDefaultAction(p->addToPlaylistAction);
    replacePlaylist->setDefaultAction(p->replacePlaylistAction);
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
    treeView->addAction(p->addToPlaylistAction);
    treeView->addAction(p->replacePlaylistAction);
    listView->addAction(p->addToPlaylistAction);
    listView->addAction(p->replacePlaylistAction);
    listView->addAction(backAction);
    listView->addAction(homeAction);
//     listView->setItemDelegate(new LibraryItemDelegate(this));
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
    connect(listView, SIGNAL(activated(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(listView, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemDoubleClicked(const QModelIndex &)));
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
    int cLevel=currentLevel;
    currentLevel=level;

    backAction->setEnabled(0!=level);
    homeAction->setEnabled(0!=level);
    if (level>cLevel) {
        prevSearch=searchList->text();
        searchList->setText(QString());
    } else {
        searchList->setText(prevSearch);
        prevSearch=QString();
    }
#ifdef ENABLE_KDE_SUPPORT
    switch(level) {
    default:
    case 0:
        searchList->setPlaceholderText(i18n("Search artists..."));
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
        searchList->setPlaceholderText(tr("Search artists..."));
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
    prevSearch=QString();
    searchList->setText(prevSearch);
}

void LibraryPage::backActivated()
{
    if (usingTreeView) {
        return;
    }
    setLevel(currentLevel-1, static_cast<MusicLibraryItem *>(proxy.mapToSource(listView->rootIndex().parent()).internalPointer()));
    listView->setRootIndex(listView->rootIndex().parent());
}

void LibraryPage::itemActivated(const QModelIndex &index)
{
    MusicLibraryItem *item = static_cast<MusicLibraryItem *>(proxy.mapToSource(index).internalPointer());

    if (MusicLibraryItem::Type_Song!=item->type()) {
        if (usingTreeView) {
            treeView->setExpanded(index, !treeView->isExpanded(index));
        } else {
            setLevel(currentLevel+1, item);
            listView->setRootIndex(index);
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
    proxy.setFilterField(usingTreeView ? 3 : currentLevel);
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
