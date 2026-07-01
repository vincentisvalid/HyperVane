#include "IPCClient.h"
#include "hypervane.pb.h"

#include <QDataStream>

static constexpr quint32 kMaxMsgLen = 64u * 1024u * 1024u; // 64 MiB

IPCClient::IPCClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    connect(m_socket, &QLocalSocket::connected,
            this, &IPCClient::onConnected);
    connect(m_socket, &QLocalSocket::readyRead,
            this, &IPCClient::onReadyRead);
    connect(m_socket, &QLocalSocket::disconnected,
            this, &IPCClient::onDisconnected);
}

void IPCClient::connectToServer(const QString &socketPath)
{
    m_socket->connectToServer(socketPath);
}

void IPCClient::onConnected()
{
    emit connected();
}

void IPCClient::sendSearchRequest(const QString &query,
                                   const QString &platform,
                                   quint32 limit,
                                   quint32 offset)
{
    hypervane::Envelope env;
    auto *req = env.mutable_search_request();
    req->set_query(query.toStdString());
    req->set_platform(platform.toStdString());
    req->set_limit(limit);
    req->set_offset(offset);

    QByteArray payload(env.ByteSizeLong(), Qt::Uninitialized);
    env.SerializeToArray(payload.data(), payload.size());
    writeFramed(payload);
}

void IPCClient::sendLaunchRequest(const QString &romId,
                                   const QString &romPath,
                                   const QString &emulatorId)
{
    hypervane::Envelope env;
    auto *req = env.mutable_launch_request();
    req->set_rom_id(romId.toStdString());
    req->set_rom_path(romPath.toStdString());
    req->set_emulator_id(emulatorId.toStdString());

    QByteArray payload(env.ByteSizeLong(), Qt::Uninitialized);
    env.SerializeToArray(payload.data(), payload.size());
    writeFramed(payload);
}

void IPCClient::sendKillRequest(quint32 pid)
{
    hypervane::Envelope env;
    env.mutable_kill_request()->set_pid(pid);

    QByteArray payload(env.ByteSizeLong(), Qt::Uninitialized);
    env.SerializeToArray(payload.data(), payload.size());
    writeFramed(payload);
}

void IPCClient::onReadyRead()
{
    m_readBuf.append(m_socket->readAll());

    while (m_readBuf.size() >= 4) {
        quint32 msgLen = 0;
        QDataStream ds(m_readBuf);
        ds.setByteOrder(QDataStream::BigEndian);
        ds >> msgLen;

        // Reject oversized frames — prevents quint32 overflow and OOM.
        if (msgLen > kMaxMsgLen) {
            m_socket->disconnectFromServer();
            return;
        }

        // Use qint64 arithmetic to avoid quint32 wrap-around on the size check.
        if (static_cast<qint64>(m_readBuf.size()) < static_cast<qint64>(4) + static_cast<qint64>(msgLen))
            break;

        QByteArray msg = m_readBuf.mid(4, static_cast<int>(msgLen));
        m_readBuf.remove(0, 4 + static_cast<int>(msgLen));
        dispatchEnvelope(msg);
    }
}

void IPCClient::onDisconnected()
{
    emit connectionLost();
}

void IPCClient::dispatchEnvelope(const QByteArray &data)
{
    hypervane::Envelope env;
    if (!env.ParseFromArray(data.constData(), data.size()))
        return;

    switch (env.payload_case()) {
    case hypervane::Envelope::kPreviewReady:
        emit previewReady(
            QString::fromStdString(env.preview_ready().rom_id()),
            QString::fromStdString(env.preview_ready().file_path()));
        break;

    case hypervane::Envelope::kScrapeProgress:
        emit scrapeProgress(
            QString::fromStdString(env.scrape_progress().rom_id()),
            env.scrape_progress().percent(),
            QString::fromStdString(env.scrape_progress().stage()),
            QString::fromStdString(env.scrape_progress().message()));
        break;

    case hypervane::Envelope::kSearchResponse: {
        QVector<RomInfo> roms;
        roms.reserve(env.search_response().results_size());
        for (const auto &e : env.search_response().results()) {
            RomInfo ri;
            ri.id       = QString::fromStdString(e.id());
            ri.title    = QString::fromStdString(e.title());
            ri.platform = QString::fromStdString(e.platform());
            ri.region   = QString::fromStdString(e.region());
            ri.filePath = QString::fromStdString(e.file_path());
            ri.verified = e.verified();
            ri.crc32    = QString::fromStdString(e.crc32());
            ri.sha1     = QString::fromStdString(e.sha1());
            ri.fileSize = e.file_size();
            roms.push_back(ri);
        }
        emit searchResultsReceived(roms);
        break;
    }

    case hypervane::Envelope::kProcessEvent:
        emit processEventReceived(
            QString::fromStdString(env.process_event().rom_id()),
            env.process_event().kind(),
            env.process_event().runtime_ms());
        break;

    default:
        break;
    }
}

void IPCClient::writeFramed(const QByteArray &payload)
{
    QByteArray frame;
    QDataStream ds(&frame, QIODevice::WriteOnly);
    ds.setByteOrder(QDataStream::BigEndian);
    ds << static_cast<quint32>(payload.size());
    frame.append(payload);

    if (m_socket->write(frame) == -1)
        emit connectionLost();
}
