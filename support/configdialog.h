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

#ifndef CONFIG_DIALOG_H
#define CONFIG_DIALOG_H

#include "dialog.h"
#include "config.h"
#include <QMap>

#ifdef __APPLE__
#include <QMainWindow>
class QToolBar;
class QToolButton;
class QButtonGroup;
class QStackedWidget;
class QDialogButtonBox;
class QPropertyAnimation;
class QAction;
#else
class PageWidget;
class PageWidgetItem;
#endif
class QIcon;
class QAbstractButton;

class ConfigDialog : public
        #ifdef __APPLE__
        QMainWindow
        #else
        Dialog
        #endif
{
    Q_OBJECT
    #ifdef __APPLE__
    Q_PROPERTY(int h READ getH WRITE setH)
    #endif

public:
    ConfigDialog(QWidget *parent, const QString &name=QString(), const QSize &defSize=QSize(), bool instantApply=false);
    ~ConfigDialog() override;

    void addPage(const QString &id, QWidget *widget, const QString &name, const QIcon &icon, const QString &header);
    bool setCurrentPage(const QString &id);
    QWidget *getPage(const QString &id) const;

    #ifdef __APPLE__
    void setCaption(const QString &c) { setWindowTitle(c); }
    void accept();
    void reject();
    int getH() const { return height(); }
    void setH(int h);
    #endif

    virtual void save()=0;
    virtual void cancel()=0;

public Q_SLOTS:
    #ifdef __APPLE__
    void slotButtonClicked(int button);
    #else
    void slotButtonClicked(int button) override;
    #endif

private Q_SLOTS:
    void activatePage();
    void macButtonPressed(QAbstractButton *b);
    void setFocus();

private:
    #ifdef __APPLE__
    void keyPressEvent(QKeyEvent *e) override;
    void showEvent(QShowEvent *e) override;
    void hideEvent(QHideEvent *e) override;
    void closeEvent(QCloseEvent *e) override;
    #endif

private:
    #ifdef __APPLE__
    struct Page {
        Page() : item(0), widget(0), index(0) { }
        QToolButton *item;
        QWidget *widget;
        int index;
    };
    QToolBar *toolBar;
    QAction *rightSpacer;
    QButtonGroup *group;
    QStackedWidget *stack;
    QDialogButtonBox *buttonBox;
    QMap<QString, Page> pages;
    bool shown;
    QPropertyAnimation *resizeAnim;
    #else
    PageWidget *pageWidget;
    QMap<QString, PageWidgetItem *> pages;
    #endif
};

#endif
