#ifndef PROXYSETTINGS_H
#define PROXYSETTINGS_H

#include <QWidget>
#include "ui_proxysettings.h"

class ProxySettings : public QWidget, public Ui::ProxySettings
{
  Q_OBJECT

public:
    ProxySettings(QWidget *parent);
    ~ProxySettings();

    void load() { }
    void save() { }
};

#endif
