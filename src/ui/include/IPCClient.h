#pragma once

#include <QObject>
#include <QLocalSocket>
#include <QString>
#include <QByteArray>
#include <QVector>
#include "RomInfo.h"

class IPCClient : public QObject
{
    Q_OBJECT

public:
    explicit IPCClient(QObject *parent = nullptr);

    void connectToServer(const QString &socketPath);
    void sendSearchRequest(const QString &query,
                           const QString &platform = {},
                           quint32 limit  = 100,
                           quint32 offset = 0);
    void sendLaunchRequest(const QString &romId,
                           const QString &romPath,
                           const QString &emulatorId);
    void sendKillRequest(quint32 pid);

signals:
    void connected();
    void previewReady(const QString &romId, const QString &filePath);
    void scrapeProgress(const QString &romId, float percent, const QString &stage, const QString &message);
    void searchResultsReceived(const QVector<RomInfo> &roms);
    void processEventReceived(const QString &romId, int kind, quint64 runtimeMs);
    void connectionLost();

private slots:
    void onConnected();
    void onReadyRead();
    void onDisconnected();

private:
    void dispatchEnvelope(const QByteArray &data);
    void writeFramed(const QByteArray &payload);

    QLocalSocket *m_socket = nullptr;
    QByteArray    m_readBuf;
};
