#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

#include "MainController.h"
#include "PlaybackControl.h"
#include "FuelUnloadingDataLoader.h"
#include "ConfigManager.h"
#include "TianDiTu/TiandituGeocoder.h"
#include "TianDiTu/TiandituRoutePlanner.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    
    // Set application properties
    app.setApplicationName("CarMove Tracker");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("CarMove");
    
    // Set Qt Quick style to Basic for better customization support
    QQuickStyle::setStyle("Material");
    
    // Register QML types for all components
    qmlRegisterType<MainController>("CarMove", 1, 0, "MainController");
    qmlRegisterType<PlaybackControl>("CarMove", 1, 0, "PlaybackControl");
    qmlRegisterType<FuelUnloadingDataLoader>("CarMove", 1, 0, "FuelUnloadingDataLoader");
    qmlRegisterType<ConfigManager>("CarMove", 1, 0, "ConfigManager");
    qmlRegisterType<TiandituGeocoder>("CarMove", 1, 0, "TiandituGeocoder");

    // Create QML engine
    QQmlApplicationEngine engine;
    
    // Create and register main controller
    MainController controller;
    engine.rootContext()->setContextProperty("controller", &controller);

    TiandituGeocoder geocoder;
    engine.rootContext()->setContextProperty("geocoder", &geocoder);
    TiandituRoutePlanner routePlanner;
    engine.rootContext()->setContextProperty("routePlanner", &routePlanner);

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
