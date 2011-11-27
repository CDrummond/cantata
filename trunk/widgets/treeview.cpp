#include "treeview.h"

TreeView::TreeView(QWidget *parent)
        : QTreeView(parent)
{
    sortByColumn(0, Qt::AscendingOrder);
    setDragEnabled(true);
    setContextMenuPolicy(Qt::NoContextMenu);
}

TreeView::~TreeView()
{
}

void TreeView::selectionChanged(const QItemSelection &selected, const QItemSelection &deselected)
{
    QTreeView::selectionChanged(selected, deselected);
    bool haveSelection=selectedIndexes().count();

    setContextMenuPolicy(haveSelection ? Qt::ActionsContextMenu : Qt::NoContextMenu);
    emit itemsSelected(haveSelection);
}

bool TreeView::haveSelectedItems() const
{
    return selectedIndexes().count()>0;
}

bool TreeView::haveUnSelectedItems() const
{
    return selectedIndexes().count()!=model()->rowCount();
}
