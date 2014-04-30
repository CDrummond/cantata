/*
 * Cantata
 *
 * Copyright (c) 2011-2014 Craig Drummond <craig.p.drummond@gmail.com>
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

#include "interfacesettings.h"
#include "settings.h"
#include "itemview.h"
#include "localize.h"
#include "musiclibraryitemalbum.h"
#include "albumsmodel.h"
#include "fancytabwidget.h"
#include "basicitemdelegate.h"
#include "playqueueview.h"
#include "pathrequester.h"
#include "flickcharm.h"
#include <QComboBox>
#ifndef ENABLE_KDE_SUPPORT
#include <QDir>
#include <QMap>
#include <QRegExp>
#include <QSet>
#endif
#ifdef QT_QTDBUS_FOUND
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

static void addAlbumSorts(QComboBox *box)
{
    box->addItem(i18n("Album, Artist, Year"), AlbumsModel::Sort_AlbumArtist);
    box->addItem(i18n("Album, Year, Artist"), AlbumsModel::Sort_AlbumYear);
    box->addItem(i18n("Artist, Album, Year"), AlbumsModel::Sort_ArtistAlbum);
    box->addItem(i18n("Artist, Year, Album"), AlbumsModel::Sort_ArtistYear);
    box->addItem(i18n("Year, Album, Artist"), AlbumsModel::Sort_YearAlbum);
    box->addItem(i18n("Year, Artist, Album"), AlbumsModel::Sort_YearArtist);
}

static void addImageSizes(QComboBox *box)
{
    box->addItem(i18n("None"), MusicLibraryItemAlbum::CoverNone);
    box->addItem(i18n("Small"), MusicLibraryItemAlbum::CoverSmall);
    box->addItem(i18n("Medium"), MusicLibraryItemAlbum::CoverMedium);
    box->addItem(i18n("Large"), MusicLibraryItemAlbum::CoverLarge);
    box->addItem(i18n("Extra Large"), MusicLibraryItemAlbum::CoverExtraLarge);
}

static QString viewTypeString(ItemView::Mode mode)
{
    switch (mode) {
    default:
    case ItemView::Mode_BasicTree:    return i18n("Basic Tree (No Icons)");
    case ItemView::Mode_SimpleTree:   return i18n("Simple Tree");
    case ItemView::Mode_DetailedTree: return i18n("Detailed Tree");
    case ItemView::Mode_GroupedTree:  return i18n("Grouped Albums");
    case ItemView::Mode_List:         return i18n("List");
    case ItemView::Mode_IconTop:      return i18n("Icon/List");
    case ItemView::Mode_Table:        return i18n("Table");
    }
}

static void addViewTypes(QComboBox *box, QList<ItemView::Mode> modes)
{
    foreach (ItemView::Mode m, modes) {
        box->addItem(viewTypeString(m), m);
    }
}

static void selectEntry(QComboBox *box, int v)
{
    for (int i=1; i<box->count(); ++i) {
        if (box->itemData(i).toInt()==v) {
            box->setCurrentIndex(i);
            return;
        }
    }
}
bool enableNotifications(true);

static inline int getValue(QComboBox *box)
{
    return box->itemData(box->currentIndex()).toInt();
}

static const char * constValueProperty="value";

InterfaceSettings::InterfaceSettings(QWidget *p)
    : QWidget(p)
    #ifndef ENABLE_KDE_SUPPORT
    , loadedLangs(false)
    #endif
{
    #ifdef QT_QTDBUS_FOUND
    // We have dbus, check that org.freedesktop.Notifications exists
    bool enableNotifications=QDBusConnection::sessionBus().interface()->isServiceRegistered("org.freedesktop.Notifications");
    #elif !defined Q_OS_WIN32 && !defined Q_OS_MAC
    // Not mac, or windows, and no dbus => no notifications!
    bool enableNotifications=false;
    #endif

    setupUi(this);
    addImageSizes(libraryCoverSize);
    QList<ItemView::Mode> standardViews=QList<ItemView::Mode>() << ItemView::Mode_BasicTree << ItemView::Mode_SimpleTree
                                                                << ItemView::Mode_DetailedTree << ItemView::Mode_List;
    addViewTypes(libraryView, QList<ItemView::Mode>() << standardViews << ItemView::Mode_IconTop);
    addViewTypes(albumsView, QList<ItemView::Mode>() << standardViews << ItemView::Mode_IconTop);
    addViewTypes(folderView, standardViews);
    addViewTypes(playlistsView, QList<ItemView::Mode>() << standardViews << ItemView::Mode_GroupedTree << ItemView::Mode_Table);
    addViewTypes(searchView, QList<ItemView::Mode>() << ItemView::Mode_List << ItemView::Mode_Table);
    addViewTypes(playQueueView, QList<ItemView::Mode>() << ItemView::Mode_List << ItemView::Mode_GroupedTree << ItemView::Mode_Table);

    addImageSizes(albumsCoverSize);
    addAlbumSorts(albumSort);

    addView(i18n("Play Queue"), QLatin1String("PlayQueuePage"));
    addView(i18n("Artists"), QLatin1String("LibraryPage"));
    addView(i18n("Albums"), QLatin1String("AlbumsPage"));
    addView(i18n("Folders"), QLatin1String("FolderPage"));
    addView(i18n("Playlists"), QLatin1String("PlaylistsPage"));
    #ifdef ENABLE_DYNAMIC
    addView(i18n("Dynamic Playlists"), QLatin1String("DynamicPage"));
    #endif
    #ifdef ENABLE_STREAMS
    addViewTypes(streamsView, standardViews);
    addView(i18n("Streams (e.g. Radio Stations)"), QLatin1String("StreamsPage"));
    #else
    REMOVE(streamsView)
    REMOVE(streamsViewLabel)
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    addViewTypes(onlineView, standardViews);
    addView(i18n("Online Services - Jamendo, Maganatune, SoundCloud, and Podcasts"), QLatin1String("OnlineServicesPage"));
    #else
    REMOVE(onlineView)
    REMOVE(onlineViewLabel)
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    addViewTypes(devicesView, standardViews);
    addView(i18n("Devices - UMS, MTP (e.g. Android), and AudioCDs"), QLatin1String("DevicesPage"));
    #else
    REMOVE(devicesView)
    REMOVE(devicesViewLabel)
    REMOVE(showDeleteAction)
    #endif
    addView(i18n("Search (via MPD)"), QLatin1String("SearchPage"));
    addView(i18n("Info - Current song information (artist, album, and lyrics)"), QLatin1String("ContextPage"));

    #if !defined ENABLE_STREAMS && !defined ENABLE_ONLINE_SERVICES && !defined ENABLE_DEVICES_SUPPORT
    tabWidget->setTabText(4, i18n("Folders"));
    folderViewLabel->setText(i18n("Style:"));
    #endif

    connect(libraryView, SIGNAL(currentIndexChanged(int)), SLOT(libraryViewChanged()));
    connect(libraryCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(libraryCoverSizeChanged()));
    connect(albumsView, SIGNAL(currentIndexChanged(int)), SLOT(albumsViewChanged()));
    connect(albumsCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(albumsCoverSizeChanged()));
    connect(playlistsView, SIGNAL(currentIndexChanged(int)), SLOT(playlistsViewChanged()));
    connect(playQueueView, SIGNAL(currentIndexChanged(int)), SLOT(playQueueViewChanged()));
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), minimiseOnClose, SLOT(setEnabled(bool)));
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), SLOT(enableStartupState()));
    connect(minimiseOnClose, SIGNAL(toggled(bool)), SLOT(enableStartupState()));
    connect(forceSingleClick, SIGNAL(toggled(bool)), SLOT(forceSingleClickChanged()));
    connect(touchFriendly, SIGNAL(toggled(bool)), SLOT(touchFriendlyChanged()));
    connect(views, SIGNAL(itemChanged(QListWidgetItem*)), SLOT(viewItemChanged(QListWidgetItem*)));

    #ifdef ENABLE_KDE_SUPPORT
    REMOVE(lang)
    REMOVE(langLabel)
    REMOVE(langNoteLabel)
    #endif

    sbStyle->addItem(i18n("Large"), FancyTabWidget::Large);
    sbStyle->addItem(i18n("Small"), FancyTabWidget::Small);
    sbStyle->addItem(i18n("Tab-bar"), FancyTabWidget::Tab);
    sbPosition->addItem(Qt::LeftToRight==layoutDirection() ? i18n("Left") : i18n("Right"), FancyTabWidget::Side);
    sbPosition->addItem(i18n("Top"), FancyTabWidget::Top);
    sbPosition->addItem(i18n("Bottom"), FancyTabWidget::Bot);
    connect(sbAutoHide, SIGNAL(toggled(bool)), SLOT(sbAutoHideChanged()));
    views->setItemDelegate(new BasicItemDelegate(views));
    playQueueBackground_none->setProperty(constValueProperty, PlayQueueView::BI_None);
    playQueueBackground_cover->setProperty(constValueProperty, PlayQueueView::BI_Cover);
    playQueueBackground_custom->setProperty(constValueProperty, PlayQueueView::BI_Custom);
    playQueueBackgroundFile->setDirMode(false);
    #ifdef ENABLE_KDE_SUPPORT
    playQueueBackgroundFile->setFilter("image/jpeg image/png");
    #else
    playQueueBackgroundFile->setFilter(i18n("Images (*.png *.jpg)"));
    #endif
    int labelWidth=qMax(fontMetrics().width(QLatin1String("100%")), fontMetrics().width(i18nc("pixels", "10px")));
    playQueueBackgroundOpacityLabel->setFixedWidth(labelWidth);
    playQueueBackgroundBlurLabel->setFixedWidth(labelWidth);
    connect(playQueueBackgroundOpacity, SIGNAL(valueChanged(int)), SLOT(setPlayQueueBackgroundOpacityLabel()));
    connect(playQueueBackgroundBlur, SIGNAL(valueChanged(int)), SLOT(setPlayQueueBackgroundBlurLabel()));
    connect(playQueueBackground_none, SIGNAL(toggled(bool)), SLOT(enablePlayQueueBackgroundOptions()));
    connect(playQueueBackground_cover, SIGNAL(toggled(bool)), SLOT(enablePlayQueueBackgroundOptions()));
    connect(playQueueBackground_custom, SIGNAL(toggled(bool)), SLOT(enablePlayQueueBackgroundOptions()));

    if (!enableNotifications) {
        REMOVE(systemTrayPopup)
    }
    FlickCharm::self()->activateOn(views);
}

void InterfaceSettings::load()
{
    libraryArtistImage->setChecked(Settings::self()->libraryArtistImage());
    selectEntry(libraryView, Settings::self()->libraryView());
    libraryCoverSize->setCurrentIndex(Settings::self()->libraryCoverSize());
    libraryYear->setChecked(Settings::self()->libraryYear());
    selectEntry(albumsView, Settings::self()->albumsView());
    albumsCoverSize->setCurrentIndex(Settings::self()->albumsCoverSize());
    selectEntry(albumSort, Settings::self()->albumSort());
    selectEntry(folderView, Settings::self()->folderView());
    selectEntry(playlistsView, Settings::self()->playlistsView());
    playListsStartClosed->setChecked(Settings::self()->playListsStartClosed());
    #ifdef ENABLE_STREAMS
    selectEntry(streamsView, Settings::self()->streamsView());
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    selectEntry(onlineView, Settings::self()->onlineView());
    #endif
    groupSingle->setChecked(Settings::self()->groupSingle());
    useComposer->setChecked(Settings::self()->useComposer());
    filteredOnly->setChecked(Settings::self()->filteredOnly());
    #ifdef ENABLE_DEVICES_SUPPORT
    showDeleteAction->setChecked(Settings::self()->showDeleteAction());
    selectEntry(devicesView, Settings::self()->devicesView());
    #endif
    selectEntry(searchView, Settings::self()->searchView());

    selectEntry(playQueueView, Settings::self()->playQueueView());
    playQueueAutoExpand->setChecked(Settings::self()->playQueueAutoExpand());
    playQueueStartClosed->setChecked(Settings::self()->playQueueStartClosed());
    playQueueScroll->setChecked(Settings::self()->playQueueScroll());

    int pqBgnd=Settings::self()->playQueueBackground();
    playQueueBackground_none->setChecked(pqBgnd==playQueueBackground_none->property(constValueProperty).toInt());
    playQueueBackground_cover->setChecked(pqBgnd==playQueueBackground_cover->property(constValueProperty).toInt());
    playQueueBackground_custom->setChecked(pqBgnd==playQueueBackground_custom->property(constValueProperty).toInt());
    playQueueBackgroundOpacity->setValue(Settings::self()->playQueueBackgroundOpacity());
    playQueueBackgroundBlur->setValue(Settings::self()->playQueueBackgroundBlur());
    playQueueBackgroundFile->setText(Settings::self()->playQueueBackgroundFile());

    playQueueConfirmClear->setChecked(Settings::self()->playQueueConfirmClear());
    albumsViewChanged();
    albumsCoverSizeChanged();
    playlistsViewChanged();
    playQueueViewChanged();
    forceSingleClick->setChecked(Settings::self()->forceSingleClick());
    touchFriendly->setChecked(Settings::self()->touchFriendly());
    systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
    if (systemTrayPopup) {
        systemTrayPopup->setChecked(Settings::self()->showPopups());
    }
    minimiseOnClose->setChecked(Settings::self()->minimiseOnClose());
    minimiseOnClose->setEnabled(systemTrayCheckBox->isChecked());
    switch (Settings::self()->startupState()) {
    case Settings::SS_ShowMainWindow: startupStateShow->setChecked(true); break;
    case Settings::SS_HideMainWindow: startupStateHide->setChecked(true); break;
    case Settings::SS_Previous: startupStateRestore->setChecked(true); break;
    }

    enableStartupState();
    cacheScaledCovers->setChecked(Settings::self()->cacheScaledCovers());
    fetchCovers->setChecked(Settings::self()->fetchCovers());

    QStringList hiddenPages=Settings::self()->hiddenPages();
    for (int i=0; i<views->count(); ++i) {
        QListWidgetItem *v=views->item(i);
        v->setCheckState(hiddenPages.contains(v->data(Qt::UserRole).toString()) ? Qt::Unchecked : Qt::Checked);
    }
    int sidebar=Settings::self()->sidebar();
    selectEntry(sbStyle, sidebar&FancyTabWidget::Style_Mask);
    selectEntry(sbPosition, sidebar&FancyTabWidget::Position_Mask);
    sbIconsOnly->setChecked(sidebar&FancyTabWidget::IconOnly);
    sbMonoIcons->setChecked(Settings::self()->monoSidebarIcons());
    sbAutoHide->setChecked(Settings::self()->splitterAutoHide());
    sbAutoHideChanged();
    viewItemChanged(views->item(0));
    setPlayQueueBackgroundOpacityLabel();
    setPlayQueueBackgroundBlurLabel();
    enablePlayQueueBackgroundOptions();
    libraryViewChanged();
}

void InterfaceSettings::save()
{
    Settings::self()->saveLibraryArtistImage(libraryArtistImage->isChecked());
    Settings::self()->saveLibraryView(getValue(libraryView));
    Settings::self()->saveLibraryCoverSize(libraryCoverSize->currentIndex());
    Settings::self()->saveLibraryYear(libraryYear->isChecked());
    Settings::self()->saveAlbumsView(getValue(albumsView));
    Settings::self()->saveAlbumsCoverSize(albumsCoverSize->currentIndex());
    Settings::self()->saveAlbumSort(getValue(albumSort));
    Settings::self()->saveFolderView(getValue(folderView));
    Settings::self()->savePlaylistsView(getValue(playlistsView));
    Settings::self()->savePlayListsStartClosed(playListsStartClosed->isChecked());
    #ifdef ENABLE_STREAMS
    Settings::self()->saveStreamsView(getValue(streamsView));
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    Settings::self()->saveOnlineView(getValue(onlineView));
    #endif
    Settings::self()->saveGroupSingle(groupSingle->isChecked());
    Settings::self()->saveUseComposer(useComposer->isChecked());
    Settings::self()->saveFilteredOnly(filteredOnly->isChecked());
    #ifdef ENABLE_DEVICES_SUPPORT
    Settings::self()->saveShowDeleteAction(showDeleteAction->isChecked());
    Settings::self()->saveDevicesView(getValue(devicesView));
    #endif
    Settings::self()->saveSearchView(getValue(searchView));
    Settings::self()->savePlayQueueView(getValue(playQueueView));
    Settings::self()->savePlayQueueAutoExpand(playQueueAutoExpand->isChecked());
    Settings::self()->savePlayQueueStartClosed(playQueueStartClosed->isChecked());
    Settings::self()->savePlayQueueScroll(playQueueScroll->isChecked());

    if (playQueueBackground_none->isChecked()) {
        Settings::self()->savePlayQueueBackground(playQueueBackground_none->property(constValueProperty).toInt());
    } else if (playQueueBackground_cover->isChecked()) {
        Settings::self()->savePlayQueueBackground(playQueueBackground_cover->property(constValueProperty).toInt());
    } else if (playQueueBackground_custom->isChecked()) {
        Settings::self()->savePlayQueueBackground(playQueueBackground_custom->property(constValueProperty).toInt());
    }
    Settings::self()->savePlayQueueBackgroundOpacity(playQueueBackgroundOpacity->value());
    Settings::self()->savePlayQueueBackgroundBlur(playQueueBackgroundBlur->value());
    Settings::self()->savePlayQueueBackgroundFile(playQueueBackgroundFile->text().trimmed());

    Settings::self()->savePlayQueueConfirmClear(playQueueConfirmClear->isChecked());
    Settings::self()->saveForceSingleClick(forceSingleClick->isChecked());
    Settings::self()->saveTouchFriendly(touchFriendly->isChecked());
    Settings::self()->saveUseSystemTray(systemTrayCheckBox->isChecked());
    if (systemTrayPopup) {
        Settings::self()->saveShowPopups(systemTrayPopup->isChecked());
    }
    Settings::self()->saveMinimiseOnClose(minimiseOnClose->isChecked());
    if (startupStateShow->isChecked()) {
        Settings::self()->saveStartupState(Settings::SS_ShowMainWindow);
    } else if (startupStateHide->isChecked()) {
        Settings::self()->saveStartupState(Settings::SS_HideMainWindow);
    } else if (startupStateRestore->isChecked()) {
        Settings::self()->saveStartupState(Settings::SS_Previous);
    }
    Settings::self()->saveCacheScaledCovers(cacheScaledCovers->isChecked());
    Settings::self()->saveFetchCovers(fetchCovers->isChecked());
    #ifndef ENABLE_KDE_SUPPORT
    if (loadedLangs && lang) {
        Settings::self()->saveLang(lang->itemData(lang->currentIndex()).toString());
    }
    #endif

    QStringList hiddenPages;
    for (int i=0; i<views->count(); ++i) {
        QListWidgetItem *v=views->item(i);
        if (Qt::Unchecked==v->checkState()) {
            hiddenPages.append(v->data(Qt::UserRole).toString());
        }
    }
    Settings::self()->saveHiddenPages(hiddenPages);
    int sidebar=getValue(sbStyle)|getValue(sbPosition);
    if (sbIconsOnly->isChecked()) {
        sidebar|=FancyTabWidget::IconOnly;
    }
    Settings::self()->saveSidebar(sidebar);
    Settings::self()->saveMonoSidebarIcons(sbMonoIcons->isChecked());
    Settings::self()->saveSplitterAutoHide(sbAutoHide->isChecked());
}

#ifndef ENABLE_KDE_SUPPORT
static bool localeAwareCompare(const QString &a, const QString &b)
{
    return a.localeAwareCompare(b) < 0;
}

static QSet<QString> translationCodes(const QString &dir)
{
    QSet<QString> codes;
    QDir d(dir);
    QStringList installed(d.entryList(QStringList() << "*.qm"));
    QRegExp langRegExp("^cantata_(.*).qm$");
    foreach (const QString &filename, installed) {
        if (langRegExp.exactMatch(filename)) {
            codes.insert(langRegExp.cap(1));
        }
    }
    return codes;
}

void InterfaceSettings::showEvent(QShowEvent *e)
{
    if (!loadedLangs) {
        loadedLangs=true;

        QMap<QString, QString> langMap;
        QSet<QString> transCodes;

        transCodes+=translationCodes(qApp->applicationDirPath()+QLatin1String("/translations"));
        transCodes+=translationCodes(QDir::currentPath()+QLatin1String("/translations"));
        #ifndef Q_OS_WIN
        transCodes+=translationCodes(INSTALL_PREFIX"/share/cantata/translations/");
        #endif

        foreach (const QString &code, transCodes) {
            QString langName = QLocale::languageToString(QLocale(code).language());
            #if QT_VERSION >= 0x040800
            QString nativeName = QLocale(code).nativeLanguageName();
            if (!nativeName.isEmpty()) {
                langName = nativeName;
            }
            #endif
            langMap[QString("%1 (%2)").arg(langName, code)] = code;
        }

        langMap["English (en)"] = "en";

        QString current = Settings::self()->lang();
        QStringList names = langMap.keys();
        qStableSort(names.begin(), names.end(), localeAwareCompare);
        lang->addItem(i18n("System default"), QString());
        lang->setCurrentIndex(0);
        foreach (const QString &name, names) {
            lang->addItem(name, langMap[name]);
            if (langMap[name]==current) {
                lang->setCurrentIndex(lang->count()-1);
            }
        }

        if (lang->count()<3) {
            REMOVE(lang)
            REMOVE(langLabel)
            REMOVE(langNoteLabel)
        } else {
            connect(lang, SIGNAL(currentIndexChanged(int)), SLOT(langChanged()));
        }
    }
    QWidget::showEvent(e);
}
#endif

void InterfaceSettings::showPage(const QString &page)
{
    if (QLatin1String("sidebar")==page) {
        tabWidget->setCurrentIndex(0);
    }
}

void InterfaceSettings::addView(const QString &v, const QString &prop)
{
    QListWidgetItem *item=new QListWidgetItem(v, views);
    item->setCheckState(Qt::Unchecked);
    item->setData(Qt::UserRole, prop);
}

void InterfaceSettings::libraryViewChanged()
{
    int vt=getValue(libraryView);
    if (ItemView::Mode_IconTop==vt && 0==libraryCoverSize->currentIndex()) {
        libraryCoverSize->setCurrentIndex(2);
    }

    bool isIcon=ItemView::Mode_IconTop==vt;
    bool isSimpleTree=ItemView::Mode_SimpleTree==vt || ItemView::Mode_BasicTree==vt;
    libraryArtistImage->setEnabled(!isIcon && !isSimpleTree);
    if (isIcon) {
        libraryArtistImage->setChecked(true);
    } else if (isSimpleTree) {
        libraryArtistImage->setChecked(false);
    }
}

void InterfaceSettings::libraryCoverSizeChanged()
{
    if (ItemView::Mode_IconTop==getValue(libraryView) && 0==libraryCoverSize->currentIndex()) {
        libraryView->setCurrentIndex(1);
    }
    if (0==libraryCoverSize->currentIndex()) {
        libraryArtistImage->setChecked(false);
    }
}

void InterfaceSettings::albumsViewChanged()
{
    if (ItemView::Mode_IconTop==getValue(albumsView) && 0==albumsCoverSize->currentIndex()) {
        albumsCoverSize->setCurrentIndex(2);
    }
}

void InterfaceSettings::albumsCoverSizeChanged()
{
    if (ItemView::Mode_IconTop==getValue(albumsView) && 0==albumsCoverSize->currentIndex()) {
        albumsView->setCurrentIndex(1);
    }
}

void InterfaceSettings::playQueueViewChanged()
{
    bool grouped=ItemView::Mode_GroupedTree==getValue(playQueueView);
    playQueueAutoExpand->setEnabled(grouped);
    playQueueStartClosed->setEnabled(grouped);
}

void InterfaceSettings::playlistsViewChanged()
{
    bool grouped=ItemView::Mode_GroupedTree==getValue(playlistsView);
    playListsStartClosed->setEnabled(grouped);
}

void InterfaceSettings::forceSingleClickChanged()
{
    singleClickLabel->setOn(forceSingleClick->isChecked()!=Settings::self()->forceSingleClick());
}

void InterfaceSettings::touchFriendlyChanged()
{
    touchFriendlyNoteLabel->setOn(touchFriendly->isChecked()!=Settings::self()->touchFriendly());
}

void InterfaceSettings::enableStartupState()
{
    startupState->setEnabled(systemTrayCheckBox->isChecked() && minimiseOnClose->isChecked());
}

void InterfaceSettings::langChanged()
{
    #ifndef ENABLE_KDE_SUPPORT
    langNoteLabel->setOn(lang->itemData(lang->currentIndex()).toString()!=Settings::self()->lang());
    #endif
}

void InterfaceSettings::viewItemChanged(QListWidgetItem *changedItem)
{
    // If this is the playqueue that has been toggled, then control auto-hide
    // i.e. can't auto-hide if playqueue is in sidebar
    if (Qt::Checked==changedItem->checkState() && changedItem==views->item(0)) {
        sbAutoHide->setChecked(false);
    }

    // Ensure we have at least 1 view checked...
    for (int i=0; i<views->count(); ++i) {
        QListWidgetItem *v=views->item(i);
        if (Qt::Checked==v->checkState()) {
            return;
        }
    }

    views->item(1)->setCheckState(Qt::Checked);
}

void InterfaceSettings::sbAutoHideChanged()
{
    if (sbAutoHide->isChecked()) {
        views->item(0)->setCheckState(Qt::Unchecked);
    }
}

void InterfaceSettings::setPlayQueueBackgroundOpacityLabel()
{
    playQueueBackgroundOpacityLabel->setText(i18nc("value%", "%1%", playQueueBackgroundOpacity->value()));
}

void InterfaceSettings::setPlayQueueBackgroundBlurLabel()
{
    playQueueBackgroundBlurLabel->setText(i18nc("pixels", "%1px", playQueueBackgroundBlur->value()));
}

void InterfaceSettings::enablePlayQueueBackgroundOptions()
{
    playQueueBackgroundOpacity->setEnabled(!playQueueBackground_none->isChecked());
    playQueueBackgroundOpacityLabel->setEnabled(playQueueBackgroundOpacity->isEnabled());
    playQueueBackgroundBlur->setEnabled(playQueueBackgroundOpacity->isEnabled());
    playQueueBackgroundBlurLabel->setEnabled(playQueueBackgroundOpacity->isEnabled());
}
