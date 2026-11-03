#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QIcon>
#include "DeviceBackend.h"
#include <hidapi.h>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("KeyFlux"));
    QCoreApplication::setApplicationName(QStringLiteral("KeyFlux"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("KeyFlux Studio"));

    if (hid_init() != 0)
        qWarning("HIDAPI initialization failed; device discovery may be unavailable.");

    DeviceBackend backend;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);
    engine.loadFromModule("KeyFlux", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    const int result = app.exec();
    // Backend/QThreads are destroyed after the event loop exits.
    // HIDAPI state is intentionally not torn down here while workers exist.
    return result;
}
