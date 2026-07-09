#pragma once

#include <QDate>
#include <QHash>
#include <QString>
#include <QStringList>

struct TrajectoryFileInfo {
    QString filePath;
    QString fileName;
    QString plateNumber;
    QDate periodStart;
    QDate periodEnd;
    bool hasPeriod = false;
};

namespace TrajectoryFileNaming {

enum class ParseMode {
    RangeOnly,
    AllPatterns,
};

QStringList excelFileFilters();

QString formatPeriodDate(const QDate& date);

QString lookupKey(const QString& plate, const QDate& startDate, const QDate& endDate);

QString fileBaseName(const QString& plate, const QDate& startDate, const QDate& endDate);

TrajectoryFileInfo parseFileName(const QString& filePath, ParseMode mode = ParseMode::AllPatterns);

QHash<QString, QString> indexRangeFiles(const QString& trajectoryDirectory, QString& errorMessage);

} // namespace TrajectoryFileNaming
