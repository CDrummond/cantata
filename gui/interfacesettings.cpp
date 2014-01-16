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
#include "fancytabwidget.h"
#include <QComboBox>
#ifndef ENABLE_KDE_SUPPORT
#include <QDir>
#include <QMap>
#include <QRegExp>
#include <QSet>
#endif

#define REMOVE(w) \
    w->setVisible(false); \
    w->deleteLater(); \
    w=0;

static void addImageSizes(QComboBox *box)
{
    box->addItem(i18n("None"), MusicLibraryItemAlbum::CoverNone);
    box->addItem(i18n("Small"), MusicLibraryItemAlbum::CoverSmall);
    box->addItem(i18n("Medium"), MusicLibraryItemAlbum::CoverMedium);
    box->addItem(i18n("Large"), MusicLibraryItemAlbum::CoverLarge);
    box->addItem(i18n("Extra Large"), MusicLibraryItemAlbum::CoverExtraLarge);
}

static void addViewTypes(QComboBox *box, bool iconMode=false, bool groupedTree=false)
{
    box->addItem(i18n("Simple Tree"), ItemView::Mode_SimpleTree);
    box->addItem(i18n("Detailed Tree"), ItemView::Mode_DetailedTree);
    if (groupedTree) {
        box->addItem(i18n("Grouped Albums"), ItemView::Mode_GroupedTree);
    }
    box->addItem(i18n("List"), ItemView::Mode_List);
    if (iconMode) {
        box->addItem(i18n("Icon/List"), ItemView::Mode_IconTop);
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

static inline int getValue(QComboBox *box)
{
    return box->itemData(box->currentIndex()).toInt();
}

static const char * constPageProperty="cantata-page";

InterfaceSettings::InterfaceSettings(QWidget *p)
    : QWidget(p)
    #ifndef ENABLE_KDE_SUPPORT
    , loadedLangs(false)
    #endif
{
    setupUi(this);
    addImageSizes(libraryCoverSize);
    addViewTypes(libraryView, true);
    addImageSizes(albumsCoverSize);
    addViewTypes(albumsView, true);
    addViewTypes(folderView);
    addViewTypes(playlistsView, false, true);

    sbPlayQueueView->setProperty(constPageProperty, "PlayQueuePage");
    sbArtistsView->setProperty(constPageProperty, "LibraryPage");
    sbAlbumsView->setProperty(constPageProperty, "AlbumsPage");
    sbFoldersView->setProperty(constPageProperty, "FolderPage");
    sbPlaylistsView->setProperty(constPageProperty, "PlaylistsPage");
    sbSearchView->setProperty(constPageProperty, "SearchPage");
    sbInfoView->setProperty(constPageProperty, "ContextPage");

    #ifdef ENABLE_DYNAMIC
    sbDynamicView->setProperty(constPageProperty, "DynamicPage");
    #else
    REMOVE(sbDynamicView)
    #endif
    #ifdef ENABLE_STREAMS
    addViewTypes(streamsView);
    sbStreamsView->setProperty(constPageProperty, "StreamsPage");
    #else
    REMOVE(streamsView)
    REMOVE(streamsViewLabel)
    REMOVE(sbStreamsView)
    #endif

    #ifdef ENABLE_ONLINE_SERVICES
    addViewTypes(onlineView);
    sbOnlineView->setProperty(constPageProperty, "OnlineServicesPage");
    #else
    REMOVE(onlineView)
    REMOVE(onlineViewLabel)
    REMOVE(sbOnlineView)
    #endif

    #ifdef ENABLE_DEVICES_SUPPORT
    addViewTypes(devicesView);
    sbDevicesView->setProperty(constPageProperty, "DevicesPage");
    #else
    REMOVE(devicesView)
    REMOVE(devicesViewLabel)
    REMOVE(showDeleteAction)
    REMOVE(sbDevicesView)
    #endif

    #if !defined ENABLE_STREAMS && !defined ENABLE_ONLINE_SERVICES && !defined ENABLE_DEVICES_SUPPORT
    tabWidget->setTabText(4, i18n("Folders"));
    folderViewLabel->setText(i18n("Style:"));
    #endif

    connect(libraryView, SIGNAL(currentIndexChanged(int)), SLOT(libraryViewChanged()));
    connect(libraryCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(libraryCoverSizeChanged()));
    connect(albumsView, SIGNAL(currentIndexChanged(int)), SLOT(albumsViewChanged()));
    connect(albumsCoverSize, SIGNAL(currentIndexChanged(int)), SLOT(albumsCoverSizeChanged()));
    connect(playQueueGrouped, SIGNAL(currentIndexChanged(int)), SLOT(playQueueGroupedChanged()));
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), minimiseOnClose, SLOT(setEnabled(bool)));
    connect(systemTrayCheckBox, SIGNAL(toggled(bool)), SLOT(enableStartupState()));
    connect(minimiseOnClose, SIGNAL(toggled(bool)), SLOT(enableStartupState()));
    connect(forceSingleClick, SIGNAL(toggled(bool)), SLOT(forceSingleClickChanged()));
    connect(sbPlayQueueView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    connect(sbArtistsView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    connect(sbAlbumsView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    connect(sbFoldersView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    connect(sbPlaylistsView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    connect(sbSearchView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    connect(sbInfoView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    #ifdef ENABLE_DYNAMIC
    connect(sbDynamicView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    #endif
    #ifdef ENABLE_STREAMS
    connect(sbStreamsView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    #endif
    #ifdef ENABLE_ONLINE_SERVICES
    connect(sbOnlineView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    #endif
    #ifdef ENABLE_DEVICES_SUPPORT
    connect(sbDevicesView, SIGNAL(toggled(bool)), SLOT(ensureMinOneView()));
    #endif

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
    connect(sbPlayQueueView, SIGNAL(toggled(bool)), SLOT(sbPlayQueueViewChanged()));
}

void InterfaceSettings::load()
{
    libraryArtistImage->setChecked(Settings::self()->libraryArtistImage());
    selectEntry(libraryView, Settings::self()->libraryView());
    libraryCoverSize->setCurrentIndex(Settings::self()->libraryCoverSize());
    libraryYear->setChecked(Settings::self()->libraryYear());
    selectEntry(albumsView, Settings::self()->albumsView());
    albumsCoverSize->setCurrentIndex(Settings::self()->albumsCoverSize());
    albumSort->setCurrentIndex(Settings::self()->albumSort());
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
    groupMultiple->setChecked(Settings::self()->groupMultiple());
    #ifdef ENABLE_DEVICES_SUPPORT
    showDeleteAction->setChecked(Settings::self()->showDeleteAction());
    selectEntry(devicesView, Settings::self()->devicesView());
    #endif
    playQueueGrouped->setCurrentIndex(Settings::self()->playQueueGrouped() ? 1 : 0);
    playQueueAutoExpand->setChecked(Settings::self()->playQueueAutoExpand());
    playQueueStartClosed->setChecked(Settings::self()->playQueueStartClosed());
    playQueueScroll->setChecked(Settings::self()->playQueueScroll());
    playQueueBackground->setChecked(Settings::self()->playQueueBackground());
    playQueueConfirmClear->setChecked(Settings::self()->playQueueConfirmClear());
    albumsViewChanged();
    albumsCoverSizeChanged();
    playListsStyleChanged();
    playQueueGroupedChanged();
    forceSingleClick->setChecked(Settings::self()->forceSingleClick());
    systemTrayCheckBox->setChecked(Settings::self()->useSystemTray());
    systemTrayPopup->setChecked(Settings::self()->showPopups());
    minimiseOnClose->setChecked(Settings::self()->minimiseOnClose());
    minimiseOnClose->setEnabled(systemTrayCheckBox->isChecked());
    switch (Settings::self()->startupState()) {
    case Settings::SS_ShowMainWindow: startupStateShow->setChecked(true); break;
    case Settings::SS_HideMainWindow: startupStateHide->setChecked(true); break;
    case Settings::SS_Previous: startupStateRestore->setChecked(true); break;
    }

    enableStartupState();
    cacheScaledCovers->setChecked(Settings::self()->cacheScaledCovers());

    QStringList hiddenPages=Settings::self()->hiddenPages();
    foreach (QObject *child, viewsGroup->children()) {
        if (qobject_cast<QCheckBox *>(child) && !hiddenPages.contains(child->property(constPageProperty).toString())) {
            static_cast<QCheckBox *>(child)->setChecked(true);
        }
    }
    int sidebar=Settings::self()->sidebar();
    selectEntry(sbStyle, sidebar&FancyTabWidget::Style_Mask);
    selectEntry(sbPosition, sidebar&FancyTabWidget::Position_Mask);
    sbIconsOnly->setChecked(sidebar&FancyTabWidget::IconOnly);
    sbMonoIcons->setChecked(Settings::self()->monoSidebarIcons());
    sbAutoHide->setChecked(Settings::self()->splitterAutoHide());
    sbAutoHideChanged();
    sbPlayQueueViewChanged();
}

void InterfaceSettings::save()
{
    Settings::self()->saveLibraryArtistImage(libraryArtistImage->isChecked());
    Settings::self()->saveLibraryView(getValue(libraryView));
    Settings::self()->saveLibraryCoverSize(libraryCoverSize->currentIndex());
    Settings::self()->saveLibraryYear(libraryYear->isChecked());
    Settings::self()->saveAlbumsView(getValue(albumsView));
    Settings::self()->saveAlbumsCoverSize(albumsCoverSize->currentIndex());
    Settings::self()->saveAlbumSort(albumSort->currentIndex());
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
    Settings::self()->saveGroupMultiple(groupMultiple->isChecked());
    #ifdef ENABLE_DEVICES_SUPPORT
    Settings::self()->saveShowDeleteAction(showDeleteAction->isChecked());
    Settings::self()->saveDevicesView(getValue(devicesView));
    #endif
    Settings::self()->savePlayQueueGrouped(1==playQueueGrouped->currentIndex());
    Settings::self()->savePlayQueueAutoExpand(playQueueAutoExpand->isChecked());
    Settings::self()->savePlayQueueStartClosed(playQueueStartClosed->isChecked());
    Settings::self()->savePlayQueueScroll(playQueueScroll->isChecked());
    Settings::self()->savePlayQueueBackground(playQueueBackground->isChecked());
    Settings::self()->savePlayQueueConfirmClear(playQueueConfirmClear->isChecked());
    Settings::self()->saveForceSingleClick(forceSingleClick->isChecked());
    Settings::self()->saveUseSystemTray(systemTrayCheckBox->isChecked());
    Settings::self()->saveShowPopups(systemTrayPopup->isChecked());
    Settings::self()->saveMinimiseOnClose(minimiseOnClose->isChecked());
    if (startupStateShow->isChecked()) {
        Settings::self()->saveStartupState(Settings::SS_ShowMainWindow);
    } else if (startupStateHide->isChecked()) {
        Settings::self()->saveStartupState(Settings::SS_HideMainWindow);
    } else if (startupStateRestore->isChecked()) {
        Settings::self()->saveStartupState(Settings::SS_Previous);
    }
    Settings::self()->saveCacheScaledCovers(cacheScaledCovers->isChecked());
    #ifndef ENABLE_KDE_SUPPORT
    if (loadedLangs && lang) {
        Settings::self()->saveLang(lang->itemData(lang->currentIndex()).toString());
    }
    #endif

    QStringList hiddenPages;
    foreach (QObject *child, viewsGroup->children()) {
        if (qobject_cast<QCheckBox *>(child) && !static_cast<QCheckBox *>(child)->isChecked()) {
            hiddenPages.append(child->property(constPageProperty).toString());
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

void InterfaceSettings::libraryViewChanged()
{
    int vt=getValue(libraryView);
    if (ItemView::Mode_IconTop==vt && 0==libraryCoverSize->currentIndex()) {
        libraryCoverSize->setCurrentIndex(2);
    }

    bool isIcon=ItemView::Mode_IconTop==vt;
    bool isSimpleTree=ItemView::Mode_SimpleTree==vt;
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

void InterfaceSettings::playQueueGroupedChanged()
{
    playQueueAutoExpand->setEnabled(1==playQueueGrouped->currentIndex());
    playQueueStartClosed->setEnabled(1==playQueueGrouped->currentIndex());
}

void InterfaceSettings::playListsStyleChanged()
{
    bool grouped=getValue(playlistsView)==ItemView::Mode_GroupedTree;
    playListsStartClosed->setEnabled(grouped);
}

void InterfaceSettings::forceSingleClickChanged()
{
    singleClickLabel->setOn(forceSingleClick->isChecked()!=Settings::self()->forceSingleClick());
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

void InterfaceSettings::ensureMinOneView()
{
    foreach (QObject *child, viewsGroup->children()) {
        if (qobject_cast<QCheckBox *>(child) && static_cast<QCheckBox *>(child)->isChecked()) {
            return;
        }
    }
    sbArtistsView->setChecked(true);
}

void InterfaceSettings::sbAutoHideChanged()
{
    if (sbAutoHide->isChecked()) {
        sbPlayQueueView->setChecked(false);
    }
}

void InterfaceSettings::sbPlayQueueViewChanged()
{
    if (sbPlayQueueView->isChecked()) {
        sbAutoHide->setChecked(false);
    }
}
