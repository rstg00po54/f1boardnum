#pragma once

#include <QByteArray>
#include <QColor>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QThread>
#include <QString>
#include <atomic>
#include <array>

// USB I/O lives exclusively in its worker thread. No QML/UI callbacks
// directly perform blocking HID operations.
class HidWorker final : public QThread
{
    Q_OBJECT
public:
    enum Kind { MultiPad = 0, Keychron = 1 };
    explicit HidWorker(Kind kind, QObject *parent = nullptr);
    ~HidWorker() override;
    void submit(const QByteArray &payload);
    void shutdown();

signals:
    void deviceStatus(int kind, bool detected, bool connected);
    void packetReceived(int kind, const QByteArray &packet);
    void consoleOutput(int kind, const QString &text);
    void diagnostic(int kind, const QString &text);

protected:
    void run() override;

private:
    Kind m_kind;
    QMutex m_mutex;
    QQueue<QByteArray> m_queue;
    std::atomic_bool m_stopping{false};
};

class DeviceBackend final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int selectedDevice READ selectedDevice WRITE setSelectedDevice NOTIFY stateChanged)
    Q_PROPERTY(bool multiConnected READ multiConnected NOTIFY stateChanged)
    Q_PROPERTY(bool keychronDetected READ keychronDetected NOTIFY stateChanged)
    Q_PROPERTY(bool keychronConnected READ keychronConnected NOTIFY stateChanged)
    Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
    Q_PROPERTY(int mode READ mode NOTIFY stateChanged)
    Q_PROPERTY(int brightness READ brightness NOTIFY stateChanged)
    Q_PROPERTY(int speed READ speed NOTIFY stateChanged)
    Q_PROPERTY(QString color READ color NOTIFY stateChanged)
    Q_PROPERTY(QString info READ info NOTIFY stateChanged)
    Q_PROPERTY(QString logs READ logs NOTIFY logsChanged)
    Q_PROPERTY(bool saved READ saved NOTIFY stateChanged)
    Q_PROPERTY(bool logControlSupported READ logControlSupported NOTIFY stateChanged)
    Q_PROPERTY(bool loggingEnabled READ loggingEnabled NOTIFY stateChanged)
public:
    explicit DeviceBackend(QObject *parent = nullptr);
    ~DeviceBackend() override;

    int selectedDevice() const { return m_selected; }
    bool multiConnected() const { return m_connected[0]; }
    bool keychronDetected() const { return m_detected[1]; }
    bool keychronConnected() const { return m_connected[1]; }
    bool connected() const { return m_connected[m_selected]; }
    int mode() const { return m_settings[m_selected].mode; }
    int brightness() const { return m_settings[m_selected].brightness; }
    int speed() const { return m_settings[m_selected].speed; }
    QString color() const { return m_settings[m_selected].color; }
    QString info() const { return m_info[m_selected]; }
    QString logs() const { return m_logs; }
    bool saved() const { return m_selected == 0 && m_multiSaved; }
    bool logControlSupported() const { return m_selected == 1 && m_keychronLogSupported; }
    bool loggingEnabled() const { return m_logging[m_selected]; }

    void setSelectedDevice(int index);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void setMode(int index);
    Q_INVOKABLE void setBrightness(int value);
    Q_INVOKABLE void setSpeed(int value);
    Q_INVOKABLE void setColor(const QString &hex);
    Q_INVOKABLE void save();
    Q_INVOKABLE void setLogging(bool enabled);
    Q_INVOKABLE void testLed(int index);
    Q_INVOKABLE void stopTest();
    Q_INVOKABLE void clearLogs();

signals:
    void stateChanged();
    void logsChanged();

private:
    struct Settings {
        int mode = 1;
        int brightness = 180;
        int speed = 5;
        QString color = QStringLiteral("#802032");
    };

    void send(int kind, const QByteArray &data);
    void received(int kind, const QByteArray &data);
    void log(const QString &line);
    void queryKeychron();
    static QByteArray packet(std::initializer_list<unsigned char> bytes);

    std::array<HidWorker *, 2> m_workers{};
    std::array<Settings, 2> m_settings;
    std::array<bool, 2> m_connected{false, false};
    std::array<bool, 2> m_detected{false, false};
    std::array<bool, 2> m_logging{false, false};
    std::array<QString, 2> m_info{
        QStringLiteral("等待 MultiPad 连接"),
        QStringLiteral("等待 Keychron V6 Max 连接")
    };
    int m_selected = 0;
    bool m_multiSaved = false;
    bool m_keychronLogSupported = false;
    QString m_logs;
};
