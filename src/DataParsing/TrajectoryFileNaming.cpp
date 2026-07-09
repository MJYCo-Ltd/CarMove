#include "DataParsing/TrajectoryFileNaming.h"

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace TrajectoryFileNaming {

namespace {

QRegularExpression rangePattern()
{
    static const QRegularExpression regex(
        u8"^([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼][A-Z][A-Z0-9]{5,6})"
        u8"-(\\d{4}-\\d{2}-\\d{2})-(\\d{4}-\\d{2}-\\d{2})\\.(xlsx|xls)$",
        QRegularExpression::CaseInsensitiveOption);
    return regex;
}

QRegularExpression singleDatePattern()
{
    static const QRegularExpression regex(
        u8"^([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼][A-Z][A-Z0-9]{5,6})"
        u8"-(\\d{4}-\\d{2}-\\d{2})\\.(xlsx|xls)$",
        QRegularExpression::CaseInsensitiveOption);
    return regex;
}

QRegularExpression plateOnlyPattern()
{
    static const QRegularExpression regex(
        u8"^([京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼][A-Z][A-Z0-9]{5,6})\\.(xlsx|xls)$",
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
    return plate.trimmed().toUpper() + QLatin1Char('|') + formatPeriodDate(startDate)
           + QLatin1Char('|') + formatPeriodDate(endDate);
}

QString fileBaseName(const QString& plate, const QDate& startDate, const QDate& endDate)
{
    return plate.trimmed() + QLatin1Char('-') + formatPeriodDate(startDate) + QLatin1Char('-')
           + formatPeriodDate(endDate);
}

TrajectoryFileInfo parseFileName(const QString& filePath, ParseMode mode)
{
    TrajectoryFileInfo parsed;
    parsed.filePath = filePath;
    parsed.fileName = QFileInfo(filePath).fileName();

    QRegularExpressionMatch match = rangePattern().match(parsed.fileName);
    if (match.hasMatch()) {
        parsed.plateNumber = match.captured(1);
        parsed.periodStart = QDate::fromString(match.captured(2), QStringLiteral("yyyy-MM-dd"));
        parsed.periodEnd = QDate::fromString(match.captured(3), QStringLiteral("yyyy-MM-dd"));
        parsed.hasPeriod = parsed.periodStart.isValid() && parsed.periodEnd.isValid();
        return parsed;
    }

    if (mode == ParseMode::RangeOnly) {
        return parsed;
    }

    match = singleDatePattern().match(parsed.fileName);
    if (match.hasMatch()) {
        parsed.plateNumber = match.captured(1);
        parsed.periodStart = QDate::fromString(match.captured(2), QStringLiteral("yyyy-MM-dd"));
        parsed.periodEnd = parsed.periodStart;
        parsed.hasPeriod = parsed.periodStart.isValid();
        return parsed;
    }

    match = plateOnlyPattern().match(parsed.fileName);
    if (match.hasMatch()) {
        parsed.plateNumber = match.captured(1);
    }

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
