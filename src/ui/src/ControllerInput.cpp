#include "ControllerInput.h"

#include <QDir>
#include <QDebug>
#include <QFile>
#include <QSettings>

#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>

// ── helpers ─────────────────────────────────────────────────────────────────
static constexpr int kEventSize = sizeof(struct input_event);

static bool isGamepadDevice(const QString &name)
{
    // Heuristic: skip mice, keyboards, accelerometers, power buttons
    static const QStringList ignore = {
        "mouse", "keyboard", "kbd", "accelerometer", "power",
        "thinkpad", "video", "lid", "hdmi", "drm"
    };
    for (const auto &pat : ignore)
        if (name.contains(pat, Qt::CaseInsensitive)) return false;
    return true;
}

// ── ctor / dtor ──────────────────────────────────────────────────────────────
ControllerInput::ControllerInput(QObject *parent)
    : QObject(parent)
{
    buildKeyMap();
    openDevices();
}

ControllerInput::~ControllerInput()
{
    closeDevices();
}

// ── device enumeration ──────────────────────────────────────────────────────
void ControllerInput::openDevices()
{
    QDir dir("/dev/input");
    QStringList entries = dir.entryList({"event*"}, QDir::System | QDir::Files);
    if (entries.isEmpty()) {
        qDebug() << "[ControllerInput] no /dev/input/event* devices found";
        m_available = false;
        return;
    }

    for (const auto &entry : entries) {
        QString path = dir.absoluteFilePath(entry);
        int fd = ::open(path.toLocal8Bit().constData(), O_RDONLY | O_NONBLOCK);
        if (fd < 0) continue;

        // Check the device name via EVIOCGNAME
        char buf[128] = {};
        if (::ioctl(fd, EVIOCGNAME(sizeof(buf)), buf) >= 0) {
            QString name = QString::fromUtf8(buf);
            if (!isGamepadDevice(name)) {
                ::close(fd);
                continue;
            }
            qDebug() << "[ControllerInput] watching" << path << "(" << name << ")";
        }

        auto *notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
        connect(notifier, &QSocketNotifier::activated, this, &ControllerInput::onEvent);

        m_devices.append({fd, path, notifier});
        m_available = true;
    }

    if (!m_available)
        qDebug() << "[ControllerInput] no gamepad-like devices found";
}

void ControllerInput::closeDevices()
{
    for (auto &d : m_devices) {
        if (d.notifier)  d.notifier->setEnabled(false);
        if (d.fd >= 0)   ::close(d.fd);
    }
    m_devices.clear();
    m_available = false;
}

// ── event reader ────────────────────────────────────────────────────────────
void ControllerInput::onEvent(int fd)
{
    struct input_event ev;
    for (;;) {
        int n = ::read(fd, &ev, kEventSize);
        if (n < static_cast<int>(kEventSize)) break;

        // We only care about key presses (EV_KEY with value 1)
        if (ev.type != EV_KEY || ev.value != 1) continue;

        auto it = m_keyMap.find(ev.code);
        if (it != m_keyMap.end())
            emit actionTriggered(it.value());
    }
}

// ── button map ──────────────────────────────────────────────────────────────
void ControllerInput::buildKeyMap()
{
    // Sony / Xbox layout — A=South, B=East, X=West, Y=North
    m_keyMap = {
        { BTN_SOUTH,   Select },
        { BTN_EAST,    Back   },
        { BTN_WEST,    Menu   },
        { BTN_NORTH,   Menu   },
        { BTN_START,   Menu   },
        { BTN_SELECT,  Back   },
        { BTN_DPAD_UP,    Up    },
        { BTN_DPAD_DOWN,  Down  },
        { BTN_DPAD_LEFT,  Left  },
        { BTN_DPAD_RIGHT, Right },
    };

    // Also map any saved custom mappings
    QSettings s("HyperVane", "ControllerMapping");
    s.beginGroup("mapping");
    // Mapping entries look like "A / Cross" → "BTN_SOUTH"
    // We skip custom overrides for now — the defaults cover standard gamepads.
    s.endGroup();
}
