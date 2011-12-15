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

#include "albumspage.h"
#include "mpdconnection.h"
#include "covers.h"
#include "musiclibraryitemsong.h"
#include <QtGui/QIcon>
#include <QtGui/QToolButton>
#include <QtGui/QFontMetrics>
#include <QtGui/QStyledItemDelegate>
#include <QtGui/QStyle>
#include <QtGui/QStyleOptionViewItem>
#include <QtGui/QPainter>
#ifdef ENABLE_KDE_SUPPORT
#include <KAction>
#include <KLocale>
#include <KActionCollection>
#else
#include <QAction>
#endif

class AlbumItemDelegate : public QStyledItemDelegate
{
public:
    static const int constBorder = 1;
    static const int constActionBorder = 2;
    static const int constActionIconSize=16;

    AlbumItemDelegate(QObject *p, AlbumsProxyModel *pr, Action *aa, Action *ra)
        : QStyledItemDelegate(p)
        , proxy(pr)
        , addAction(aa)
        , replaceAction(ra)
    {
    }

    virtual ~AlbumItemDelegate()
    {
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        Q_UNUSED(option)
        Q_UNUSED(index)
        int imgSize=AlbumsModel::coverPixels();
        int textHeight = QApplication::fontMetrics().height()*2;

        return QSize(imgSize + (constBorder * 2),
                     textHeight+imgSize + (constBorder*2));
    }

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
    {
        if (!index.isValid()) {
            return;
        }
        QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, painter, 0L);

        QString artist = index.data(Qt::UserRole+1).toString();
        QString album = index.data(Qt::UserRole+2).toString();
        QPixmap pix = index.data(Qt::DecorationRole).value<QPixmap>();
        QRect r(option.rect);

        r.adjust(constBorder, constBorder, -constBorder, -constBorder);

        QRect r2(r);

        if (!pix.isNull()) {
            painter->drawPixmap(r.x()+((r.width()-pix.width())/2), r.y(), pix.width(), pix.height(), pix);
            r.adjust(0, pix.height()+3, 0, -3);
        }
        QFont mainFont(QApplication::font());
        QFont subFont(QApplication::font());
        QFontMetrics mainMetrics(mainFont);
        subFont.setPointSizeF(subFont.pointSizeF()*0.8);
        QFontMetrics subMetrics(subFont);
        bool selected=option.state&QStyle::State_Selected;
        QColor color(QApplication::palette().color(selected ? QPalette::HighlightedText : QPalette::Text));
        QRect mainRect;
        QRect subRect;
        QTextOption textOpt(Qt::AlignVCenter|Qt::AlignHCenter);
        int mainHeight=mainMetrics.height();
        int subHeight=subMetrics.height();
        int totalHeight=mainHeight+subHeight;
        mainRect=QRect(r.x(), r.y()+((r.height()-totalHeight)/2), r.width(), mainHeight);
        subRect=QRect(r.x(), r.y()+mainHeight+((r.height()-totalHeight)/2), r.width(), mainHeight);
        album = mainMetrics.elidedText(album, Qt::ElideRight, mainRect.width(), QPalette::WindowText);

        painter->setPen(color);
        painter->setFont(mainFont);
        painter->drawText(mainRect, album, textOpt);

        artist = subMetrics.elidedText(artist, Qt::ElideRight, subRect.width(), QPalette::WindowText);
        color.setAlphaF(selected ? 0.65 : 0.5);
        painter->setPen(color);
        painter->setFont(subFont);
        painter->drawText(subRect, artist, textOpt);

        if (option.state & QStyle::State_MouseOver) {
            if (r2.height()>(constActionIconSize+(2*constActionBorder))) {
                pix=replaceAction->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
                if (!pix.isNull()) {
                    painter->drawPixmap(r2.x()+r2.width()-(pix.width()+constActionBorder),
                                        r2.y()+constActionBorder,
                                        pix.width(), pix.height(), pix);
                    r2.adjust(0, pix.height()+constActionBorder, 0, -(pix.height()+constActionBorder));
                }
            }

            if (r2.height()>(constActionIconSize+(2*constActionBorder))) {
                pix=addAction->icon().pixmap(QSize(constActionIconSize, constActionIconSize));
                if (!pix.isNull()) {
                    painter->drawPixmap(r2.x()+r2.width()-(pix.width()+constActionBorder),
                                        r2.y()+constActionBorder,
                                        pix.width(), pix.height(), pix);
                }
            }
        }
    }

    AlbumsProxyModel *proxy;
    Action *addAction;
    Action *replaceAction;
};

AlbumsPage::AlbumsPage(MainWindow *p)
    : QWidget(p)
{
    setupUi(this);
    addToPlaylistAction=p->addToPlaylistAction;
    replacePlaylistAction=p->replacePlaylistAction;
    addToPlaylist->setDefaultAction(addToPlaylistAction);
    replacePlaylist->setDefaultAction(replacePlaylistAction);
    libraryUpdate->setDefaultAction(p->updateDbAction);
    connect(view, SIGNAL(itemsSelected(bool)), addToPlaylist, SLOT(setEnabled(bool)));
    connect(view, SIGNAL(itemsSelected(bool)), replacePlaylist, SLOT(setEnabled(bool)));

    addToPlaylist->setAutoRaise(true);
    replacePlaylist->setAutoRaise(true);
    libraryUpdate->setAutoRaise(true);
    addToPlaylist->setEnabled(false);
    replacePlaylist->setEnabled(false);

#ifdef ENABLE_KDE_SUPPORT
    search->setPlaceholderText(i18n("Search albums..."));
#else
    search->setPlaceholderText(tr("Search albums..."));
#endif
    view->addAction(addToPlaylistAction);
    view->addAction(replacePlaylistAction);
    view->addAction(p->addToStoredPlaylistAction);
    proxy.setSourceModel(&model);
    view->setModel(&proxy);
    view->setItemDelegate(new AlbumItemDelegate(this, &proxy, addToPlaylistAction, replacePlaylistAction));

    connect(this, SIGNAL(add(const QStringList &)), MPDConnection::self(), SLOT(add(const QStringList &)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
    connect(search, SIGNAL(returnPressed()), this, SLOT(searchItems()));
    connect(search, SIGNAL(textChanged(const QString)), this, SLOT(searchItems()));
    connect(genreCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(searchItems()));
    connect(view, SIGNAL(doubleClicked(const QModelIndex &)), this, SLOT(itemActivated(const QModelIndex &)));
    connect(view, SIGNAL(clicked(const QModelIndex &)),  this, SLOT(itemClicked(const QModelIndex &)));
    connect(Covers::self(), SIGNAL(cover(const QString &, const QString &, const QImage &)),
            &model, SLOT(setCover(const QString &, const QString &, const QImage &)));
    clear();
}

AlbumsPage::~AlbumsPage()
{
}

void AlbumsPage::clear()
{
    QFontMetrics fm(font());

    int size=AlbumsModel::coverPixels();
    view->setIconSize(QSize(size, size));
    view->setGridSize(QSize(size+8, size+(fm.height()*2.5)));
    AlbumsModel::setItemSize(view->gridSize()-QSize(4, 4));
    model.clear();
    view->update();
}

void AlbumsPage::addSelectionToPlaylist(const QString &name)
{
    QStringList files;

    const QModelIndexList selected = view->selectionModel()->selectedIndexes();

    if (0==selected.size()) {
        return;
    }

    foreach (const QModelIndex &idx, selected) {
        QStringList albumFiles=model.data(proxy.mapToSource(idx), Qt::UserRole).toStringList();

        foreach (const QString & file, albumFiles) {
            if (!files.contains(file)) {
                files.append(file);
            }
        }
    }

    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->selectionModel()->clearSelection();
    }
}

void AlbumsPage::itemClicked(const QModelIndex &index)
{
    QRect rect(view->visualRect(index));
    rect.moveTo(view->viewport()->mapToGlobal(QPoint(rect.x(), rect.y())));
    QRect actionRect(rect.x()+rect.width()-(AlbumItemDelegate::constActionIconSize+AlbumItemDelegate::constActionBorder),
                     rect.y()+AlbumItemDelegate::constActionBorder,
                     AlbumItemDelegate::constActionIconSize, AlbumItemDelegate::constActionIconSize);
    if (actionRect.contains(QCursor::pos())) {
        replacePlaylistAction->trigger();
        return;
    }
    actionRect.adjust(0, AlbumItemDelegate::constActionIconSize+AlbumItemDelegate::constActionBorder,
                      0, AlbumItemDelegate::constActionIconSize+AlbumItemDelegate::constActionBorder);
    if (actionRect.contains(QCursor::pos())) {
        addToPlaylistAction->trigger();
    }
}

void AlbumsPage::itemActivated(const QModelIndex &)
{
    if (1==view->selectionModel()->selectedIndexes().size()) {//doubleclick should only have one selected item
        addSelectionToPlaylist();
    }
}

void AlbumsPage::searchItems()
{
    proxy.setFilterGenre(0==genreCombo->currentIndex() ? QString() : genreCombo->currentText());

    if (search->text().isEmpty()) {
        proxy.setFilterRegExp(QString());
        return;
    }

    proxy.setFilterRegExp(search->text());
}

void AlbumsPage::updateGenres(const QStringList &genres)
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
