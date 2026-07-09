#pragma once

#include <QString>

struct PostGisDatabaseConfig {
    QString host = QStringLiteral("localhost");
    int port = 5432;
    QString database = QStringLiteral("carmove");
    QString username = QStringLiteral("postgres");
    QString password;
    QString schema = QStringLiteral("public");
    QString trajectoryTable = QStringLiteral("trajectory_points");
    QString trajectoryDaysTable = QStringLiteral("trajectory_days");
    QString vehiclesTable = QStringLiteral("vehicles");
    QString plateColumn = QStringLiteral("plate_number");
    QString timeColumn = QStringLiteral("ts");
    QString geomColumn = QStringLiteral("geom");
    QString speedColumn = QStringLiteral("speed");
    QString directionColumn = QStringLiteral("direction");
    QString colorColumn = QStringLiteral("plate_color");

    bool isValid() const;
    QString connectionSummary() const;
};
