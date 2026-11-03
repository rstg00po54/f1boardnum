#include "DeviceBackend.h"

#include <QElapsedTimer>
#include <QMutexLocker>
#include <QtMath>
#include <hidapi/hidapi.h>

#include <algorithm>
#include <array>
#include <cstring>

namespace {
constexpr unsigned short kVendorPage = 0xFF60;
constexpr unsigned short kVendorUsage = 0x61;
constexpr unsigned short kConsolePage = 0xFF31;
constexpr unsigned short kConsoleUsage = 0x74;

unsigned short vidFor(int kind) { return kind == 0 ? 0x0483 : 0x3434; }
unsigned short pidFor(int kind) { return kind == 0 ? 0x572B : 0x0960; }
int byteAt(const QByteArray &b, int i)
{
    return i >= 0 && i < b.size() ? static_cast<unsigned char>(b.at(i)) : 0;
}
QString niceName(int kind) { return kind == 0 ? QStringLiteral("MultiPad") : QStringLiteral("Keychron V6 Max"); }
} // namespace

HidWorker::HidWorker(Kind kind, QObject *parent) : QThread(parent), m_kind(kind) {}
HidWorker::~HidWorker() { shutdown(); }

void HidWorker::submit(const QByteArray &payload)
{
    if (payload.size() != 32) return;
    QMutexLocker lock(&m_mutex);
    // Bound the queue if a slider is dragged very quickly or the device
    // is disconnected. We never queue infinite stale USB commands.
    if (m_queue.size() >= 48) m_queue.dequeue();
    m_queue.enqueue(payload);
}

void HidWorker::shutdown()
{
    m_stopping.store(true);
    if (isRunning()) wait();
}

void HidWorker::run()
{
    const int kind = static_cast<int>(m_kind);
    hid_device *control = nullptr;
    hid_device *console = nullptr;
    bool wasDetected = false;
    bool wasConnected = false;

    QElapsedTimer timer;
    timer.start();
    qint64 lastScan = -2000;
    qint64 lastPoll = 0;

    while (!m_stopping.load()) {
        const qint64 now = timer.elapsed();

        if (!control && now - lastScan >= 1600) {
            lastScan = now;
            bool found = false;
            QByteArray rawPath, consolePath;
            hid_device_info *head = hid_enumerate(vidFor(kind), pidFor(kind));
            for (const hid_device_info *it = head; it; it = it->next) {
                found = true;
                if (it->usage_page == kVendorPage && it->usage == kVendorUsage && rawPath.isEmpty())
                    rawPath = QByteArray(it->path);
                if (kind == 1 && it->usage_page == kConsolePage && it->usage == kConsoleUsage)
                    consolePath = QByteArray(it->path);
            }
            hid_free_enumeration(head);

            if (found != wasDetected) {
                wasDetected = found;
                emit deviceStatus(kind, found, false);
            }

            if (!rawPath.isEmpty())
                control = hid_open_path(rawPath.constData());

            if (control && kind == 1 && !consolePath.isEmpty()) {
                console = hid_open_path(consolePath.constData());
                if (console) hid_set_nonblocking(console, 1);
            }
            if (control) {
                wasConnected = true;
                lastPoll = now - 4000;
                emit deviceStatus(kind, true, true);
                emit diagnostic(kind, niceName(kind) + QStringLiteral(" HID 控制接口已打开"));
            } else if (found && now < 4000) {
                emit diagnostic(kind, niceName(kind) + QStringLiteral(" 已发现，但未发现可打开的 0xFF60/0x61 控制接口"));
            }
        }

        if (!control) {
            msleep(70);
            continue;
        }

        if (now - lastPoll >= 4000) {
            lastPoll = now;
            // Enqueue polling through exactly the same serialized path.
            QByteArray poll(32, '\0');
            poll[0] = kind == 0 ? char(0x01) : char(0x08);
            if (kind == 1) { poll[1] = 3; poll[2] = 1; }
            submit(poll);
        }

        QByteArray out;
        {
            QMutexLocker lock(&m_mutex);
            if (!m_queue.isEmpty()) out = m_queue.dequeue();
        }
        if (!out.isEmpty()) {
            unsigned char report[33] = {};
            // Windows HIDAPI output reports include an explicit Report ID.
            // The firmware defines no report ID, so its value is zero.
            std::memcpy(report + 1, out.constData(), 32);
            if (hid_write(control, report, sizeof(report)) < 0) {
                emit diagnostic(kind, niceName(kind) + QStringLiteral(" USB 写入失败，正在重连"));
                hid_close(control);
                control = nullptr;
            }
        }

        if (!control) {
            if (console) { hid_close(console); console = nullptr; }
            if (wasConnected) { wasConnected = false; emit deviceStatus(kind, wasDetected, false); }
            continue;
        }

        unsigned char in[65] = {};
        const int size = hid_read_timeout(control, in, sizeof(in), 24);
        if (size < 0) {
            emit diagnostic(kind, niceName(kind) + QStringLiteral(" USB 断开，等待重新连接"));
            hid_close(control);
            control = nullptr;
            if (console) { hid_close(console); console = nullptr; }
            {
                QMutexLocker lock(&m_mutex);
                m_queue.clear();
            }
            if (wasConnected) { wasConnected = false; emit deviceStatus(kind, wasDetected, false); }
            continue;
        }
        if (size > 0) {
            const int start = (size == 33 && in[0] == 0) ? 1 : 0;
            emit packetReceived(kind, QByteArray(reinterpret_cast<const char *>(in + start), size - start));
        }

        if (console) {
            // QMK console is a separate vendor HID interface, not the VIA
            // Raw HID channel. It is optional (depends on flashed firmware).
            for (int i = 0; i < 4; ++i) {
                unsigned char bytes[128] = {};
                int length = hid_read_timeout(console, bytes, sizeof(bytes), 0);
                if (length < 0) { hid_close(console); console = nullptr; break; }
                if (!length) break;
                int offset = (length > 32 && bytes[0] == 0) ? 1 : 0;
                QByteArray text(reinterpret_cast<const char *>(bytes + offset), length - offset);
                text = text.split('\0').first();
                if (!text.isEmpty()) emit consoleOutput(kind, QString::fromUtf8(text));
            }
        }
    }

    if (console) hid_close(console);
    if (control) hid_close(control);
}

DeviceBackend::DeviceBackend(QObject *parent) : QObject(parent)
{
    m_settings[1].mode = 1;
    m_settings[1].brightness = 170;
    m_settings[1].speed = 120;
    m_settings[1].color = QStringLiteral("#5aa9ff");
    log(QStringLiteral("KeyFlux Studio / USB HID / Qt Quick\n等待设备连接…"));

    for (int i = 0; i < 2; ++i) {
        auto *worker = new HidWorker(static_cast<HidWorker::Kind>(i), this);
        m_workers[i] = worker;
        connect(worker, &HidWorker::deviceStatus, this,
                [this](int kind, bool detected, bool active) {
            m_detected[kind] = detected;
            m_connected[kind] = active;
            if (active) {
                m_info[kind] = kind == 0
                    ? QStringLiteral("USB / 自定义 HID / 协议 v3")
                    : QStringLiteral("USB / VIA Raw HID");
                log(QStringLiteral("%1 已连接").arg(niceName(kind)));
                if (kind == 0) send(0, packet({0x01}));
                else {
                    send(1, packet({0x01}));
                    queryKeychron();
                }
            } else {
                m_info[kind] = detected
                    ? QStringLiteral("设备存在，但控制通道不可用")
                    : QStringLiteral("未连接 · USB 模式可获得完整控制功能");
                if (kind == 1) m_keychronLogSupported = false;
            }
            emit stateChanged();
        });
        connect(worker, &HidWorker::packetReceived, this,
                [this](int kind, const QByteArray &p) { received(kind, p); });
        connect(worker, &HidWorker::consoleOutput, this,
                [this](int kind, const QString &p) {
            log(QStringLiteral("[%1] %2").arg(niceName(kind), p).trimmed());
        });
        connect(worker, &HidWorker::diagnostic, this,
                [this](int, const QString &p) { log(p); });
        worker->start();
    }
}

DeviceBackend::~DeviceBackend()
{
    for (auto *worker : m_workers) worker->shutdown();
    hid_exit();
}

QByteArray DeviceBackend::packet(std::initializer_list<unsigned char> bytes)
{
    QByteArray p(32, '\0');
    int i = 0;
    for (unsigned char b : bytes) {
        if (i >= p.size()) break;
        p[i++] = static_cast<char>(b);
    }
    return p;
}

void DeviceBackend::send(int kind, const QByteArray &data)
{
    if (kind < 0 || kind > 1 || !m_connected[kind]) return;
    m_workers[kind]->submit(data);
}

void DeviceBackend::setSelectedDevice(int index)
{
    if (index < 0 || index > 1 || m_selected == index) return;
    m_selected = index;
    emit stateChanged();
}

void DeviceBackend::queryKeychron()
{
    for (unsigned char id : {1, 2, 3, 4})
        send(1, packet({0x08, 0x03, id}));
    // Custom channel exists only in the user's customized Keychron QMK.
    send(1, packet({0x08, 0xF1, 0x01, 0, 'V', '6', 'L', 'G'}));
}

void DeviceBackend::refresh()
{
    if (m_selected == 0) send(0, packet({0x01}));
    else {
        send(1, packet({0x01}));
        queryKeychron();
    }
}

void DeviceBackend::setMode(int index)
{
    if (!connected()) return;
    if (m_selected == 0) {
        if (index < 0 || index > 4) return;
        send(0, packet({0x02, static_cast<unsigned char>(index)}));
    } else {
        if (index < 0 || index > 24) return;
        send(1, packet({0x07, 0x03, 0x02, static_cast<unsigned char>(index)}));
    }
    m_settings[m_selected].mode = index;
    if (m_selected == 0) m_multiSaved = false;
    emit stateChanged();
}
void DeviceBackend::setBrightness(int value)
{
    if (!connected()) return;
    value = std::clamp(value, 0, 255);
    if (m_selected == 0)
        send(0, packet({0x04, static_cast<unsigned char>(value)}));
    else
        send(1, packet({0x07, 0x03, 0x01, static_cast<unsigned char>(value)}));
    m_settings[m_selected].brightness = value;
    if (m_selected == 0) m_multiSaved = false;
    emit stateChanged();
}
void DeviceBackend::setSpeed(int value)
{
    if (!connected()) return;
    value = std::clamp(value, m_selected == 0 ? 1 : 0, m_selected == 0 ? 10 : 255);
    if (m_selected == 0)
        send(0, packet({0x05, static_cast<unsigned char>(value)}));
    else
        send(1, packet({0x07, 0x03, 0x03, static_cast<unsigned char>(value)}));
    m_settings[m_selected].speed = value;
    if (m_selected == 0) m_multiSaved = false;
    emit stateChanged();
}
void DeviceBackend::setColor(const QString &hex)
{
    if (!connected()) return;
    const QColor c(hex);
    if (!c.isValid()) return;
    if (m_selected == 0)
        send(0, packet({0x03,
            static_cast<unsigned char>(c.red()),
            static_cast<unsigned char>(c.green()),
            static_cast<unsigned char>(c.blue())}));
    else {
        // QMK VIA RGB matrix expects hue and saturation, each 0..255.
        const int hue = c.hsvHue() < 0 ? 0 : qRound(c.hsvHue() * 255.0 / 359.0);
        const int sat = c.hsvSaturation();
        send(1, packet({0x07, 0x03, 0x04,
            static_cast<unsigned char>(hue),
            static_cast<unsigned char>(sat)}));
    }
    m_settings[m_selected].color = c.name();
    if (m_selected == 0) m_multiSaved = false;
    emit stateChanged();
}
void DeviceBackend::save()
{
    if (!connected()) return;
    if (m_selected == 0) send(0, packet({0x08})); // Flash, CRC, A/B pages
    else send(1, packet({0x09, 0x03}));            // VIA RGB matrix EEPROM
    log(QStringLiteral("%1：已发送保存命令，等待设备确认").arg(niceName(m_selected)));
}
void DeviceBackend::setLogging(bool enabled)
{
    if (!connected()) return;
    if (m_selected == 0) send(0, packet({0x06, static_cast<unsigned char>(enabled)}));
    else if (m_keychronLogSupported)
        send(1, packet({0x07, 0xF1, 0x01, static_cast<unsigned char>(enabled), 'V', '6', 'L', 'G'}));
    else {
        log(QStringLiteral("Keychron 当前固件不支持自定义日志开关"));
        return;
    }
    m_logging[m_selected] = enabled;
    emit stateChanged();
}
void DeviceBackend::testLed(int index)
{
    if (m_selected != 0 || !connected() || index < 0 || index > 99) return;
    send(0, packet({0x07, 0x01, static_cast<unsigned char>(index), 48, 0, 0}));
}
void DeviceBackend::stopTest()
{
    if (m_selected == 0 && connected()) send(0, packet({0x07, 0, 0, 0, 0, 0}));
}
void DeviceBackend::clearLogs() { m_logs.clear(); emit logsChanged(); }

void DeviceBackend::log(const QString &line)
{
    m_logs += line + (line.endsWith('\n') ? QString() : QStringLiteral("\n"));
    if (m_logs.size() > 18000) m_logs.remove(0, m_logs.size() - 13000);
    emit logsChanged();
}

void DeviceBackend::received(int kind, const QByteArray &p)
{
    if (p.isEmpty()) return;
    const int op = byteAt(p, 0);
    auto &st = m_settings[kind];

    if (kind == 0) {
        if (op == 0x80 && p.size() > 2) {
            const int count = std::min(byteAt(p, 1), p.size() - 2);
            if (count > 0) log(QString::fromUtf8(p.constData() + 2, count));
            return;
        }
        if (op == 0x81 && p.size() >= 20) {
            st.mode = byteAt(p, 2);
            st.brightness = byteAt(p, 3);
            st.color = QColor(byteAt(p, 4), byteAt(p, 5), byteAt(p, 6)).name();
            st.speed = byteAt(p, 7);
            m_logging[0] = byteAt(p, 8) != 0;
            m_multiSaved = byteAt(p, 18) != 0 && byteAt(p, 19) != 0;
            m_info[0] = QStringLiteral("协议 v%1 · MCP %2/%3 · LED %4 · %5")
                .arg(byteAt(p, 1)).arg(byteAt(p, 9)).arg(byteAt(p, 10))
                .arg(byteAt(p, 12)).arg(m_multiSaved ? QStringLiteral("已保存") : QStringLiteral("未保存"));
            emit stateChanged();
            return;
        }
        if (op == 0x82) {
            if (byteAt(p, 1) == 0x08) {
                log(byteAt(p, 2) == 1 ? QStringLiteral("MultiPad：Flash 保存成功")
                                      : QStringLiteral("MultiPad：Flash 保存失败"));
                send(0, packet({0x01}));
            } else if (byteAt(p, 2) != 1) {
                log(QStringLiteral("MultiPad：命令 %1 被拒绝").arg(byteAt(p, 1)));
            }
            return;
        }
        return;
    }

    if (op == 0x01 && p.size() >= 3) {
        m_info[1] = QStringLiteral("VIA v%1 · 0x3434:0x0960 · RGB Matrix")
            .arg((byteAt(p, 1) << 8) | byteAt(p, 2));
        emit stateChanged();
        return;
    }
    if ((op == 0x08 || op == 0x07) && byteAt(p, 1) == 0x03) {
        const int id = byteAt(p, 2);
        if (id == 1) st.brightness = byteAt(p, 3);
        if (id == 2) st.mode = byteAt(p, 3);
        if (id == 3) st.speed = byteAt(p, 3);
        if (id == 4) {
            const int hue = qRound(byteAt(p, 3) * 359.0 / 255.0);
            st.color = QColor::fromHsv(hue, byteAt(p, 4), 255).name();
        }
        emit stateChanged();
        return;
    }
    if ((op == 0x08 || op == 0x07) && byteAt(p, 1) == 0xF1
        && byteAt(p, 2) == 0x01 && p.size() >= 8
        && p[4] == 'V' && p[5] == '6' && p[6] == 'L' && p[7] == 'G') {
        m_keychronLogSupported = true;
        m_logging[1] = byteAt(p, 3) != 0;
        emit stateChanged();
        return;
    }
    if (op == 0x09 && byteAt(p, 1) == 0x03) {
        log(QStringLiteral("Keychron：VIA RGB 配置已提交到键盘"));
        return;
    }
}
