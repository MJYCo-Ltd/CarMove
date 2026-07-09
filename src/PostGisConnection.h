#pragma once

#include "PostGisDatabaseConfig.h"

#include <QString>
#include <QStringList>

namespace PostGisConnection {

bool ensureDriverAvailable(QString& errorMessage);

bool validateIdentifier(const QString& identifier, QString& errorMessage);

QStringList sqlIdentifiers(const PostGisDatabaseConfig& config);

bool validateConfigIdentifiers(const PostGisDatabaseConfig& config, QString& errorMessage);

QString makeConnectionName(const QString& prefix);

bool openDatabase(const PostGisDatabaseConfig& config,
                  const QString& connectionName,
                  QString& errorMessage);

void closeDatabase(const QString& connectionName);

} // namespace PostGisConnection
