#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QDir>
#include <QStandardPaths>
#include <QtQml>
#include <QQuickStyle>

#include "MainController.h"
#include "VehicleDataModel.h"
#include "VehicleAnimationEngine.h"
#include "FolderScanner.h"
#include "VehicleManager.h"
#include "CoordinateConverter.h"
#include "FuelUnloadingDataLoader.h"
#include "ConfigManager.h"

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
    qmlRegisterType<VehicleDataModel>("CarMove", 1, 0, "VehicleDataModel");
    qmlRegisterType<VehicleAnimationEngine>("CarMove", 1, 0, "VehicleAnimationEngine");
    qmlRegisterType<FolderScanner>("CarMove", 1, 0, "FolderScanner");
    qmlRegisterType<VehicleManager>("CarMove", 1, 0, "VehicleManager");
    qmlRegisterType<FuelUnloadingDataLoader>("CarMove", 1, 0, "FuelUnloadingDataLoader");
    qmlRegisterType<ConfigManager>("CarMove", 1, 0, "ConfigManager");
    
    // Register uncreatable types (utility classes)
    qmlRegisterUncreatableType<CoordinateConverter>("CarMove", 1, 0, "CoordinateConverter", 
                                                   "CoordinateConverter is a utility class");
    
    // Create QML engine
    QQmlApplicationEngine engine;
    
    // Create and register main controller
    MainController controller;
    engine.rootContext()->setContextProperty("controller", &controller);
    
    // Set default data path for easier access
    QString defaultDataPath = QDir::currentPath() + "/carData";
    engine.rootContext()->setContextProperty("defaultDataPath", defaultDataPath);
    
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
