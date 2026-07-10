#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QtLocation/QGeoServiceProvider>

#include "UI/MainController.h"
#include "Core/ConfigManager.h"
#include "Business/ExcelPreviewModel.h"
#include "Core/AppLogger.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("CarMove Tracker");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CarMove");

    AppLogger::initialize();
    
    // Set Qt Quick style to Basic for better customization support
    QQuickStyle::setStyle("Material");
    
    // Register QML types for all components
    qmlRegisterType<MainController>("CarMove", 1, 0, "MainController");
    qmlRegisterType<ConfigManager>("CarMove", 1, 0, "ConfigManager");
    qmlRegisterType<ExcelPreviewModel>("CarMove", 1, 0, "ExcelPreviewModel");

    // Create QML engine
    QQmlApplicationEngine engine;

    // QGroundControl 1.0 (MapTileMonitor) 由 QGCLocation geoservice 插件在 setQmlEngine 时注册。
    // 须在加载 QML 前触发，否则 import QGroundControl 1.0 会报 module is not installed。
    QGeoServiceProvider qgcGeoService(QStringLiteral("QGroundControl"));
    qgcGeoService.setQmlEngine(&engine);
    if (!qgcGeoService.mappingManager()) {
        qWarning() << "QGroundControl geoservice unavailable:"
                   << qgcGeoService.mappingErrorString();
    }

    // Create and register main controller
    MainController controller;
    engine.rootContext()->setContextProperty("controller", &controller);

    QObject::connect(&app, &QGuiApplication::aboutToQuit, [&controller]() {
        controller.prepareForApplicationShutdown();
        AppLogger::shutdown();
    });

    // 地图服务：经 MapServiceManager 统一管理，保留 geocoder/routePlanner 别名以兼容现有 QML
    if (controller.mapService()) {
        engine.rootContext()->setContextProperty("mapService", controller.mapService());
        engine.rootContext()->setContextProperty("geocoder", controller.mapService()->geocoder());
        engine.rootContext()->setContextProperty("routePlanner", controller.mapService()->routePlanner());
    }

    // Load main QML file
    const QUrl url(QStringLiteral("qrc:/MainWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    
    engine.load(url);
    
    return app.exec();
}
