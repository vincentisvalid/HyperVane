#pragma once

#include <QObject>
#include <QSocketNotifier>
#include <QHash>
#include <QVector>
#include <QString>

class ControllerInput : public QObject
{
    Q_OBJECT

public:
    enum Action { Up, Down, Left, Right, Select, Back, Menu };

    explicit ControllerInput(QObject *parent = nullptr);
    ~ControllerInput() override;

    bool isAvailable() const { return m_available; }

signals:
    void actionTriggered(ControllerInput::Action action);

private slots:
    void onEvent(int fd);

private:
    void openDevices();
    void closeDevices();

    struct Device {
        int fd = -1;
        QString path;
        QSocketNotifier *notifier = nullptr;
    };

    QVector<Device> m_devices;
    bool m_available = false;

    // evdev key code → action mapping (standard gamepad layout)
    QHash<quint16, Action> m_keyMap;
    void buildKeyMap();
};
