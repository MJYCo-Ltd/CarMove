#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlError>
#include <QQuickStyle>
#include <QtLocation/QGeoServiceProvider>

#include "UI/MainController.h"
#include "Core/ConfigManager.h"
#include "Business/ExcelPreviewModel.h"
#include "Core/AppLogger.h"

namespace {

void logStartupFailure(const QString& message)
{
    AppLogger::critical(message);
    AppLogger::flush();
}

void installQmlWarningLogger(QQmlApplicationEngine& engine)
{
    QObject::connect(
        &engine,
        &QQmlEngine::warnings,
        &engine,
        [](const QList<QQmlError>& warnings) {
            for (const QQmlError& error : warnings) {
                AppLogger::error(QStringLiteral("QML 错误: %1").arg(error.toString()));
            }
            if (!warnings.isEmpty()) {
                AppLogger::flush();
            }
        });
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    app.setApplicationName("CarMove Tracker");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CarMove");

    AppLogger::initialize();
    AppLogger::info(QStringLiteral("应用启动 | argc=%1").arg(argc));

    QQuickStyle::setStyle("Material");

    qmlRegisterType<MainController>("CarMove", 1, 0, "MainController");
    qmlRegisterType<ConfigManager>("CarMove", 1, 0, "ConfigManager");
    qmlRegisterType<ExcelPreviewModel>("CarMove", 1, 0, "ExcelPreviewModel");

    QQmlApplicationEngine engine;
    installQmlWarningLogger(engine);

    QGeoServiceProvider qgcGeoService(QStringLiteral("QGroundControl"));
    qgcGeoService.setQmlEngine(&engine);
    if (!qgcGeoService.mappingManager()) {
        AppLogger::warn(QStringLiteral("QGroundControl 地图服务不可用: %1")
                            .arg(qgcGeoService.mappingErrorString()));
    }

    MainController controller;
    engine.rootContext()->setContextProperty("controller", &controller);

    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&controller]() {
        controller.prepareForApplicationShutdown();
        AppLogger::info(QStringLiteral("应用即将退出"));
        AppLogger::shutdown();
    });

    if (controller.mapService()) {
        engine.rootContext()->setContextProperty("mapService", controller.mapService());
        engine.rootContext()->setContextProperty("geocoder", controller.mapService()->geocoder());
        engine.rootContext()->setContextProperty("routePlanner", controller.mapService()->routePlanner());
    } else {
        AppLogger::warn(QStringLiteral("MapService 未就绪，地图相关功能可能不可用"));
    }

    const QUrl url(QStringLiteral("qrc:/MainWindow.qml"));

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        [](const QUrl& objUrl) {
            logStartupFailure(QStringLiteral("QML 对象创建失败: %1").arg(objUrl.toString()));
        });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &app,
        [url](QObject* obj, const QUrl& objUrl) {
            if (url != objUrl) {
                return;
            }

            if (!obj) {
                logStartupFailure(QStringLiteral("主界面 QML 创建失败: %1").arg(objUrl.toString()));
                QCoreApplication::exit(-1);
                return;
            }

            AppLogger::info(QStringLiteral("主界面 QML 创建成功: %1").arg(objUrl.toString()));
        },
        Qt::QueuedConnection);

    AppLogger::info(QStringLiteral("开始加载 QML: %1").arg(url.toString()));
    engine.load(url);
    AppLogger::info(QStringLiteral("engine.load 已返回 | rootObjects=%1")
                        .arg(engine.rootObjects().size()));
    AppLogger::flush();

    const QList<QObject*> rootObjects = engine.rootObjects();
    if (rootObjects.isEmpty()) {
        logStartupFailure(QStringLiteral("QML 加载后无根对象，界面无法显示 | url=%1").arg(url.toString()));
        AppLogger::shutdown();
        return -1;
    }

    AppLogger::info(QStringLiteral("QML 根对象数量=%1，进入事件循环").arg(rootObjects.size()));
    const int exitCode = app.exec();

    if (exitCode != 0) {
        AppLogger::warn(QStringLiteral("应用退出 | exitCode=%1").arg(exitCode));
        AppLogger::flush();
    }

    return exitCode;
}
