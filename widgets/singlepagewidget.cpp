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

#include "singlepagewidget.h"
#include "widgets/spacerwidget.h"
#include "widgets/toolbutton.h"
#include "widgets/icons.h"
#include "gui/stdactions.h"
#include "gui/application.h"
#include "mpd-interface/mpdconnection.h"
#include <QGridLayout>
#include <QHBoxLayout>

static QString viewTypeString(ItemView::Mode mode)
{
    switch (mode) {
    default:
    case ItemView::Mode_BasicTree:    return QObject::tr("Basic Tree (No Icons)");
    case ItemView::Mode_SimpleTree:   return QObject::tr("Simple Tree");
    case ItemView::Mode_DetailedTree: return QObject::tr("Detailed Tree");
    case ItemView::Mode_GroupedTree:  return QObject::tr("Grouped Albums");
    case ItemView::Mode_List:         return QObject::tr("List");
    case ItemView::Mode_IconTop:      return QObject::tr("Grid");
    case ItemView::Mode_Table:        return QObject::tr("Table");
    #ifdef ENABLE_CATEGORIZED_VIEW
    case ItemView::Mode_Categorized:  return QObject::tr("Categorized");
    #endif
    }
}

SinglePageWidget::SinglePageWidget(QWidget *p)
    : QWidget(p)
    , btnFlags(0)
    , refreshAction(nullptr)
{
    QGridLayout *layout=new QGridLayout(this);
    view=new ItemView(this);
    QWidget *sizer=new QWidget(this);
    Application::fixSize(sizer);
    layout->addWidget(view, 1, 0, 1, 5);
    layout->addWidget(sizer, 2, 2, 1, 1);
    layout->setMargin(0);
    layout->setSpacing(0);
    connect(view, SIGNAL(searchItems()), this, SIGNAL(searchItems()));
    connect(view, SIGNAL(itemsSelected(bool)), this, SLOT(controlActions()));
    connect(this, SIGNAL(add(const QStringList &, int, quint8, bool)), MPDConnection::self(), SLOT(add(const QStringList &, int, quint8, bool)));
    connect(this, SIGNAL(addSongsToPlaylist(const QString &, const QStringList &)), MPDConnection::self(), SLOT(addToPlaylist(const QString &, const QStringList &)));
}

void SinglePageWidget::addWidget(QWidget *w)
{
    Application::fixSize(w);
    static_cast<QGridLayout *>(layout())->addWidget(w, 0, 0, 1, 5);
}

void SinglePageWidget::init(int flags, const QList<QWidget *> &leftXtra, const QList<QWidget *> &rightXtra)
{
    if (0!=btnFlags) {
        return;
    }
    btnFlags=flags;
    QList<QWidget *> left=leftXtra;
    QList<QWidget *> right=rightXtra;

    if (!right.isEmpty() && (flags&(AppendToPlayQueue|ReplacePlayQueue))) {
        right << new SpacerWidget(this);
    }

    if (flags&ReplacePlayQueue) {
        view->addAction(StdActions::self()->replacePlayQueueAction);
    }

    if (flags&AppendToPlayQueue) {
        ToolButton *addToPlayQueue=new ToolButton(this);
        addToPlayQueue->setDefaultAction(StdActions::self()->appendToPlayQueueAction);
        right.append(addToPlayQueue);
        view->addAction(StdActions::self()->addToPlayQueueMenuAction);
    }

    if (flags&ReplacePlayQueue) {
        ToolButton *replacePlayQueue=new ToolButton(this);
        replacePlayQueue->setDefaultAction(StdActions::self()->replacePlayQueueAction);
        right.append(replacePlayQueue);
    }

    if (flags&Refresh) {
        ToolButton *refreshButton=new ToolButton(this);
        refreshAction=new Action(Icons::self()->reloadIcon, tr("Refresh"), this);
        refreshButton->setDefaultAction(refreshAction);
        connect(refreshAction, SIGNAL(triggered()), this, SLOT(refresh()));
        left.append(refreshButton);
    }

    connect(this, SIGNAL(searchItems()), this, SLOT(doSearch()));

    if (!left.isEmpty()) {
        QHBoxLayout *ll=new QHBoxLayout();
        ll->setMargin(0);
        ll->setSpacing(1);
        for (QWidget *b: left) {
            Application::fixSize(b);
            ll->addWidget(b);
        }
        static_cast<QGridLayout *>(layout())->addItem(ll, 2, 0, 1, 1);
    }
    if (!right.isEmpty()) {
        QHBoxLayout *rl=new QHBoxLayout();
        rl->setMargin(0);
        rl->setSpacing(1);
        for (QWidget *b: right) {
            Application::fixSize(b);
            rl->addWidget(b);
        }
        static_cast<QGridLayout *>(layout())->addItem(rl, 2, 4, 1, 1);
    }
}

void SinglePageWidget::addSelectionToPlaylist(const QString &name, int action, quint8 priority, bool decreasePriority)
{
    // Always get tracks and playlists - this way error message can be shown. #902
    QStringList files=selectedFiles(true);
    if (!files.isEmpty()) {
        if (name.isEmpty()) {
            emit add(files, action, priority, decreasePriority);
        } else {
            emit addSongsToPlaylist(name, files);
        }
        view->clearSelection();
    }
}

void SinglePageWidget::showEvent(QShowEvent *e)
{
    QWidget::showEvent(e);
    controlActions();
    view->focusView();
}

void SinglePageWidget::hideEvent(QHideEvent *e)
{
    QWidget::hideEvent(e);
    controlActions();
}

const char * SinglePageWidget::constValProp="val";

QList<QAction *> SinglePageWidget::createActions(const QList<SinglePageWidget::MenuItem> &values,int currentVal, QWidget *parent, const char *slot)
{
    QList<QAction *> actions;
    QActionGroup *group=new QActionGroup(parent);
    for (const MenuItem &v: values) {
        QAction *act=new QAction(v.first, parent);
        connect(act, SIGNAL(toggled(bool)), parent, slot);
        act->setActionGroup(group);
        act->setProperty(constValProp, v.second);
        act->setCheckable(true);
        act->setChecked(v.second==currentVal);
        actions.append(act);
    }
    return actions;
}

Action * SinglePageWidget::createMenuGroup(const QString &name, const QList<QAction *> actions, QWidget *parent)
{
    Action *action=new Action(name, parent);
    QMenu *menu=new QMenu(parent);
    menu->addActions(actions);
    action->setMenu(menu);
    return action;
}

Action * SinglePageWidget::createMenuGroup(const QString &name, const QList<SinglePageWidget::MenuItem> &values,int currentVal, QWidget *parent, const char *slot)
{
    return createMenuGroup(name, createActions(values, currentVal, parent, slot), parent);
}

QList<QAction *> SinglePageWidget::createViewActions(QList<ItemView::Mode> modes)
{
    QList<QPair<QString, int> > vals;
    for (ItemView::Mode m: modes) {
        #ifndef ENABLE_CATEGORIZED_VIEW
        if (m!=ItemView::Mode_Categorized)
        #endif
        vals.append(MenuItem(viewTypeString(m), m));
    }
    return createActions(vals, view->viewMode(), this, SLOT(viewModeSelected()));
}

Action * SinglePageWidget::createViewMenu(QList<ItemView::Mode> modes)
{
    return createMenuGroup(tr("View"), createViewActions(modes), this);
}

void SinglePageWidget::setView(int v)
{
    view->setMode((ItemView::Mode)v);
}

void SinglePageWidget::viewModeSelected()
{
    QAction *act=qobject_cast<QAction *>(sender());
    if (act) {
        setView(act->property(constValProp).toInt());
    }
}

void SinglePageWidget::focusSearch()
{
    view->focusSearch();
}

void SinglePageWidget::controlActions()
{
    QModelIndexList selected=view->selectedIndexes(false);
    if (btnFlags&ReplacePlayQueue) {
        StdActions::self()->replacePlayQueueAction->setEnabled(!selected.isEmpty());
    }
    if (btnFlags&AppendToPlayQueue) {
        StdActions::self()->appendToPlayQueueAction->setEnabled(!selected.isEmpty());
    }
    StdActions::self()->addRandomAlbumToPlayQueueAction->setVisible(false);
}

#include "moc_singlepagewidget.cpp"
