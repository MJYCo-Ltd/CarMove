#pragma once

#include "DataManagement/PostGisDatabaseConfig.h"

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QTime>
#include <QTimeZone>

namespace PostGisSql {

inline QTimeZone shanghaiTimeZone()
{
    return QTimeZone(QByteArray("Asia/Shanghai"));
}

inline QDate calendarDate(const QDateTime& timestamp)
{
    if (!timestamp.isValid()) {
        return {};
    }
    return timestamp.toTimeZone(shanghaiTimeZone()).date();
}

inline QTime calendarTime(const QDateTime& timestamp)
{
    if (!timestamp.isValid()) {
        return {};
    }
    return timestamp.toTimeZone(shanghaiTimeZone()).time();
}

inline QDateTime calendarDateTime(const QDate& date, const QTime& time)
{
    if (!date.isValid() || !time.isValid()) {
        return {};
    }
    return QDateTime(date, time, shanghaiTimeZone());
}

inline QString quotedIdentifier(const QString& identifier)
{
    return QLatin1Char('"') + identifier + QLatin1Char('"');
}

inline QString qualifiedTable(const PostGisDatabaseConfig& config, const QString& tableName)
{
    return quotedIdentifier(config.schema) + QLatin1Char('.') + quotedIdentifier(tableName);
}

} // namespace PostGisSql
