#include "MainWindow.h"
#include "LibraryView.h"
#include "PreviewPlayer.h"
#include "IPCClient.h"
#include "IntroOverlay.h"

#include <QFile>
#include <QResizeEvent>
#include <QSplitter>
#include <QTimer>
#include <QVBoxLayout>

static constexpr int  kReconnectDelayMs = 3000;
static constexpr char kSocketPath[]     = "/tmp/hypervane.sock";
static constexpr char kIntroVideo[]     =
    HYPERVANE_ASSETS_DIR "/platform_intros/hypervane_intro.mp4";

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_libraryView(std::make_unique<LibraryView>(this))
    , m_previewPlayer(std::make_unique<PreviewPlayer>(this))
    , m_ipcClient(std::make_unique<IPCClient>(this))
{
    setWindowTitle("HyperVane");
    resize(1600, 900);
    setupLayout();
    connectSignals();
    m_ipcClient->connectToServer(kSocketPath);
    showIntro();
}

MainWindow::~MainWindow() = default;

void MainWindow::setupLayout()
{
    auto *splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(m_libraryView.get());
    splitter->addWidget(m_previewPlayer.get());
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    setCentralWidget(splitter);
}

void MainWindow::connectSignals()
{
    connect(m_ipcClient.get(), &IPCClient::connected,
            this, &MainWindow::onIPCConnected);
    connect(m_ipcClient.get(), &IPCClient::connectionLost,
            this, &MainWindow::onConnectionLost);
    connect(m_ipcClient.get(), &IPCClient::searchResultsReceived,
            this, &MainWindow::onSearchResults);
    connect(m_ipcClient.get(), &IPCClient::previewReady,
            this, &MainWindow::onPreviewReady);
    connect(m_libraryView.get(), &LibraryView::romSelected,
            this, &MainWindow::onRomSelected);
    connect(m_libraryView.get(), &LibraryView::launchRequested,
            this, &MainWindow::onLaunchRequested);
}

void MainWindow::showIntro()
{
    const QString path = QString::fromLatin1(kIntroVideo);
    if (!QFile::exists(path))
        return;

    m_introOverlay = new IntroOverlay(path, this);
    connect(m_introOverlay, &IntroOverlay::finished,
            m_introOverlay, &QWidget::deleteLater);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_introOverlay)
        m_introOverlay->setGeometry(rect());
}

void MainWindow::onIPCConnected()
{
    m_ipcClient->sendSearchRequest({});
}

void MainWindow::onConnectionLost()
{
    m_libraryView->setRoms({});
    m_previewPlayer->stopPreview();
    setWindowTitle("HyperVane — reconnecting…");
    QTimer::singleShot(kReconnectDelayMs, this, [this] {
        setWindowTitle("HyperVane");
        m_ipcClient->connectToServer(kSocketPath);
    });
}

void MainWindow::onSearchResults(const QVector<RomInfo> &roms)
{
    m_libraryView->setRoms(roms);
}

void MainWindow::onRomSelected(const QString & /*romId*/, const QString & /*romPath*/)
{
    // Future: request metadata / scrape preview for this ROM
}

void MainWindow::onPreviewReady(const QString & /*romId*/, const QString &filePath)
{
    m_previewPlayer->loadPreview(filePath);
}

void MainWindow::onLaunchRequested(const QString &romId,
                                   const QString &romPath,
                                   const QString &emulatorId)
{
    m_ipcClient->sendLaunchRequest(romId, romPath, emulatorId);
}
