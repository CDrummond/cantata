#ifndef OUTPUTSETTINGS_H
#define OUTPUTSETTINGS_H

#include "ui_outputsettings.h"
#include "output.h"
#include <QtCore/QList>

class OutputSettings : public QWidget, private Ui::OutputSettings
{
    Q_OBJECT

public:
    OutputSettings(QWidget *p);
    virtual ~OutputSettings() { }

    void load();
    void save();
private Q_SLOTS:
    void updateOutpus(const QList<Output> &outputs);
};

#endif
