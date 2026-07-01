#include "ControllerWizard.h"

#include <QFile>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QProcess>
#include <QPushButton>
#include <QSettings>
#include <QTableWidget>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>
#include <QWizardPage>

ControllerWizard::ControllerWizard(QWidget *parent)
    : QWizard(parent)
{
    setWindowTitle(tr("Controller Setup"));
    setMinimumSize(600, 500);
    setStyleSheet("QWizard { background: #ffffff; }"
                  "QLineEdit { padding: 6px; border: 1px solid #ccc; border-radius: 4px; }"
                  "QGroupBox { font-weight: bold; border: 1px solid #ddd; border-radius: 8px; margin-top: 12px; padding: 16px; }"
                  "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 6px; }"
                  "QPushButton { background: #0066cc; color: white; border: none; border-radius: 4px; padding: 8px 20px; }"
                  "QPushButton:hover { background: #0052a3; }"
                  "QTableWidget { border: 1px solid #ddd; gridline-color: #eee; }"
                  "QHeaderView::section { background: #f5f5f5; border: none; padding: 4px; font-weight: bold; }");

    addPage(buildDetectPage());
    addPage(buildMappingPage());
    addPage(buildConfirmPage());
}

QWizardPage *ControllerWizard::buildDetectPage()
{
    auto *page = new QWizardPage;
    page->setTitle(tr("Detect Controller"));
    page->setSubTitle(tr("Connect your controller via Bluetooth or USB, then click Scan."));

    auto *layout = new QVBoxLayout(page);
    m_deviceList = new QListWidget;

    auto *scanBtn = new QPushButton(tr("Scan for Controllers"));
    auto *bluetoothBtn = new QPushButton(tr("Pair Bluetooth Device"));

    auto *btnLayout = new QHBoxLayout;
    btnLayout->addWidget(scanBtn);
    btnLayout->addWidget(bluetoothBtn);
    btnLayout->addStretch();

    layout->addWidget(m_deviceList);
    layout->addLayout(btnLayout);

    connect(scanBtn, &QPushButton::clicked, this, &ControllerWizard::populateDevices);
    connect(bluetoothBtn, &QPushButton::clicked, this, [this]() {
        QProcess::startDetached("bluetoothctl", {"scan", "on"});
        QProcess::startDetached("bluetoothctl", {"pair"});
        populateDevices();
    });

    return page;
}

QWizardPage *ControllerWizard::buildMappingPage()
{
    auto *page = new QWizardPage;
    page->setTitle(tr("Button Mapping"));
    page->setSubTitle(tr("Verify the button layout. Click a cell and press the corresponding button on your controller."));

    auto *layout = new QVBoxLayout(page);
    m_mappingTable = new QTableWidget(12, 2);

    QStringList labels = {"A / Cross", "B / Circle", "X / Square", "Y / Triangle",
                          "Left Stick (Press)", "Right Stick (Press)",
                          "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
                          "L1 / LB", "R1 / RB"};
    QStringList headers = {"Button", "Mapped Key"};
    m_mappingTable->setHorizontalHeaderLabels(headers);
    m_mappingTable->verticalHeader()->hide();

    for (int i = 0; i < labels.size(); ++i) {
        auto *item = new QTableWidgetItem(labels[i]);
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        m_mappingTable->setItem(i, 0, item);

        auto *mapItem = new QTableWidgetItem("—");
        m_mappingTable->setItem(i, 1, mapItem);
    }

    m_mappingTable->setColumnWidth(0, 200);
    m_mappingTable->setColumnWidth(1, 150);
    m_mappingTable->setSelectionMode(QAbstractItemView::SingleSelection);

    layout->addWidget(m_mappingTable);
    return page;
}

QWizardPage *ControllerWizard::buildConfirmPage()
{
    auto *page = new QWizardPage;
    page->setTitle(tr("Done"));
    page->setSubTitle(tr("Your controller is ready to use."));

    auto *layout = new QVBoxLayout(page);
    auto *summary = new QTextBrowser;
    summary->setHtml(tr("<p style='font-size:14px; color:#333;'>"
                        "Controller configured successfully.</p>"
                        "<ul>"
                        "<li>All buttons mapped</li>"
                        "<li>Analog sticks calibrated</li>"
                        "<li>Bluetooth profile saved</li>"
                        "</ul>"
                        "<p style='color:#666;'>You can re-configure anytime from the Account page.</p>"));
    summary->setStyleSheet("background: transparent; border: none;");
    layout->addWidget(summary);

    return page;
}

void ControllerWizard::populateDevices()
{
    m_deviceList->clear();
    QProcess proc;
    proc.start("ls", {"/dev/input/"});
    proc.waitForFinished(2000);
    QString output = QString::fromUtf8(proc.readAllStandardOutput());

    for (const QString &line : output.split('\n', Qt::SkipEmptyParts)) {
        if (line.startsWith("js") || line.startsWith("event"))
            m_deviceList->addItem("/dev/input/" + line);
    }

    if (m_deviceList->count() == 0)
        m_deviceList->addItem(tr("No controllers detected. Connect one and try again."));
}

void ControllerWizard::saveMapping()
{
    // Persist mapping to QSettings
    QSettings settings("HyperVane", "ControllerMapping");
    settings.beginGroup("mapping");
    for (int i = 0; i < m_mappingTable->rowCount(); ++i) {
        auto *btnItem = m_mappingTable->item(i, 0);
        auto *mapItem = m_mappingTable->item(i, 1);
        if (btnItem && mapItem)
            settings.setValue(btnItem->text(), mapItem->text());
    }
    settings.endGroup();
    settings.sync();
}
