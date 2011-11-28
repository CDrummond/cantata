#include "outputsettings.h"
#include "lib/mpdconnection.h"
#include <QtGui/QListWidget>

OutputSettings::OutputSettings(QWidget *p)
    : QWidget(p)
{
    setupUi(this);
    connect(MPDConnection::self(), SIGNAL(outputsUpdated(const QList<Output> &)), this, SLOT(updateOutpus(const QList<Output> &)));
};

void OutputSettings::load()
{
    MPDConnection::self()->outputs();
}

void OutputSettings::save()
{
    for (int i=0; i<view->count(); ++i) {
        QListWidgetItem *item=view->item(i);

        if (Qt::Checked==item->checkState()) {
            MPDConnection::self()->enableOutput(item->data(Qt::UserRole).toInt());
        } else {
            MPDConnection::self()->disableOutput(item->data(Qt::UserRole).toInt());
        }
    }
}

void OutputSettings::updateOutpus(const QList<Output> &outputs)
{
    view->clear();
    foreach(const Output &output, outputs) {
        QListWidgetItem *item=new QListWidgetItem(output.name, view);
        item->setCheckState(output.enabled ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, output.id);
    }
}
