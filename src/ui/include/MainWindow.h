#pragma once

#include <QMainWindow>
#include <QPointer>
#include <QVector>
#include <memory>
#include "RomInfo.h"

class LibraryView;
class PreviewPlayer;
class IPCClient;
class IntroOverlay;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void onIPCConnected();
    void onConnectionLost();
    void onRomSelected(const QString &romId, const QString &romPath);
    void onPreviewReady(const QString &romId, const QString &filePath);
    void onSearchResults(const QVector<RomInfo> &roms);
    void onLaunchRequested(const QString &romId, const QString &romPath, const QString &emulatorId);

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupLayout();
    void connectSignals();
    void showIntro();

    std::unique_ptr<LibraryView>   m_libraryView;
    std::unique_ptr<PreviewPlayer> m_previewPlayer;
    std::unique_ptr<IPCClient>     m_ipcClient;
    QPointer<IntroOverlay>         m_introOverlay;
};
