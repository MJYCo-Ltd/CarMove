#include "PostGisDatabaseConfig.h"

bool PostGisDatabaseConfig::isValid() const
{
    return !host.trimmed().isEmpty()
           && port > 0
           && !database.trimmed().isEmpty()
           && !username.trimmed().isEmpty()
           && !schema.trimmed().isEmpty()
           && !trajectoryTable.trimmed().isEmpty()
           && !trajectoryDaysTable.trimmed().isEmpty()
           && !vehiclesTable.trimmed().isEmpty()
           && !plateColumn.trimmed().isEmpty()
           && !timeColumn.trimmed().isEmpty()
           && !geomColumn.trimmed().isEmpty();
}

QString PostGisDatabaseConfig::connectionSummary() const
{
    return QStringLiteral("%1@%2:%3/%4").arg(username, host).arg(port).arg(database);
}
