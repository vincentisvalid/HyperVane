#pragma once

#include <QWizard>

class QListWidget;
class QTableWidget;

class ControllerWizard : public QWizard
{
    Q_OBJECT
public:
    explicit ControllerWizard(QWidget *parent = nullptr);

private:
    QWizardPage *buildDetectPage();
    QWizardPage *buildMappingPage();
    QWizardPage *buildConfirmPage();

    void populateDevices();
    void saveMapping();

    QListWidget  *m_deviceList   = nullptr;
    QTableWidget *m_mappingTable = nullptr;
};
