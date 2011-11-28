#ifndef INTERFACESETTINGS_H
#define INTERFACESETTINGS_H

#include "ui_interfacesettings.h"
#include <QtGui/QCheckBox>

class InterfaceSettings : public QWidget, private Ui::InterfaceSettings
{
public:
    InterfaceSettings(QWidget *p);
    virtual ~InterfaceSettings() { }

    void load();
    void save();

    bool sysTrayEnabled() const { return systemTrayCheckBox->isChecked(); }
};

#endif
