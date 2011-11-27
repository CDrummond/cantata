#ifndef TREEVIEW_H
#define TREEVIEW_H

#include <QtGui/QTreeView>

class TreeView : public QTreeView
{
    Q_OBJECT

public:
    TreeView(QWidget *parent=0);

    virtual ~TreeView();

    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    bool haveSelectedItems() const;
    bool haveUnSelectedItems() const;

Q_SIGNALS:
    bool itemsSelected(bool);
};

#endif
