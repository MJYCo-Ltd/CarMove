#include "DataParsing/TrajectoryFileNaming.h"

#include "DataParsing/LicensePlate.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace TrajectoryFileNaming {

namespace {

QRegularExpression fileNamePattern()
{
    static const QRegularExpression regex(
        QStringLiteral("^") + LicensePlate::capturePattern()
            + QStringLiteral(R"((?:-(\d{4}-\d{2}-\d{2})(?:-(\d{4}-\d{2}-\d{2}))?)?\.(xlsx|xls)$)"),
        QRegularExpression::CaseInsensitiveOption);
    return regex;
}

} // namespace

QStringList excelFileFilters()
{
    return {QStringLiteral("*.xlsx"),
            QStringLiteral("*.xls"),
            QStringLiteral("*.XLSX"),
            QStringLiteral("*.XLS")};
}

QString formatPeriodDate(const QDate& date)
{
    if (!date.isValid()) {
        return QString();
    }
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}

QString lookupKey(const QString& plate, const QDate& startDate, const QDate& endDate)
{
    return LicensePlate::canonicalPlateNumber(plate).toUpper() + QLatin1Char('|')
           + formatPeriodDate(startDate) + QLatin1Char('|') + formatPeriodDate(endDate);
}

QString fileBaseName(const QString& plate, const QDate& startDate, const QDate& endDate)
{
    return LicensePlate::canonicalPlateNumber(plate) + QLatin1Char('-') + formatPeriodDate(startDate)
           + QLatin1Char('-') + formatPeriodDate(endDate);
}

TrajectoryFileInfo parseFileName(const QString& filePath, ParseMode mode)
{
    TrajectoryFileInfo parsed;
    parsed.filePath = filePath;
    parsed.fileName = QFileInfo(filePath).fileName();

    const QRegularExpressionMatch match = fileNamePattern().match(parsed.fileName);
    if (!match.hasMatch()) {
        return parsed;
    }

    parsed.plateNumber = LicensePlate::canonicalPlateNumber(match.captured(1));
    if (parsed.plateNumber.isEmpty() || !LicensePlate::isChineseVehiclePlate(parsed.plateNumber)) {
        parsed.plateNumber.clear();
        return parsed;
    }

    const QString startText = match.captured(2);
    const QString endText = match.captured(3);
    if (startText.isEmpty() || (mode == ParseMode::RangeOnly && endText.isEmpty())) {
        if (mode == ParseMode::RangeOnly) {
            parsed.plateNumber.clear();
        }
        return parsed;
    }

    parsed.periodStart = QDate::fromString(startText, QStringLiteral("yyyy-MM-dd"));
    parsed.periodEnd = endText.isEmpty()
                           ? parsed.periodStart
                           : QDate::fromString(endText, QStringLiteral("yyyy-MM-dd"));
    parsed.hasPeriod = parsed.periodStart.isValid() && parsed.periodEnd.isValid();
    return parsed;
}

QHash<QString, QString> indexRangeFiles(const QString& trajectoryDirectory, QString& errorMessage)
{
    QHash<QString, QString> index;
    errorMessage.clear();

    const QFileInfo dirInfo(trajectoryDirectory);
    if (!dirInfo.exists() || !dirInfo.isDir()) {
        errorMessage = QStringLiteral("轨迹目录无效: %1").arg(trajectoryDirectory);
        return index;
    }

    const QDir dir(trajectoryDirectory);
    const QFileInfoList files = dir.entryInfoList(excelFileFilters(), QDir::Files | QDir::Readable);

    for (const QFileInfo& fileInfo : files) {
        const TrajectoryFileInfo parsed = parseFileName(fileInfo.absoluteFilePath(), ParseMode::RangeOnly);
        if (!parsed.hasPeriod) {
            continue;
        }

        const QString key = lookupKey(parsed.plateNumber, parsed.periodStart, parsed.periodEnd);
        if (!index.contains(key)) {
            index.insert(key, fileInfo.absoluteFilePath());
        }
    }

    return index;
}

} // namespace TrajectoryFileNaming
