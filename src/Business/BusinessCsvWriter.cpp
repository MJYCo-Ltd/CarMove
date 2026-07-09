#include "Business/BusinessCsvWriter.h"

#include <QFile>
#include <QStringConverter>
#include <QTextStream>

namespace {

QString csvField(const QString& value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));

    if (escaped.contains(QLatin1Char(',')) || escaped.contains(QLatin1Char('"'))
        || escaped.contains(QLatin1Char('\n')) || escaped.contains(QLatin1Char('\r'))) {
        return QLatin1Char('"') + escaped + QLatin1Char('"');
    }

    return escaped;
}

} // namespace

QString BusinessCsvWriter::formatExportDate(const QDate& date)
{
    if (!date.isValid()) {
        return QString();
    }
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}

bool BusinessCsvWriter::writeRowsToCsv(const QList<BusinessExportRow>& rows,
                                       const QString& filePath,
                                       QString& errorMessage)
{
    errorMessage.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        errorMessage = QStringLiteral("无法写入文件: %1").arg(filePath);
        return false;
    }

    file.write("\xEF\xBB\xBF");

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << QStringLiteral("车牌,开始时间,结束时间\n");

    for (const BusinessExportRow& row : rows) {
        stream << csvField(row.plate) << QLatin1Char(',')
               << csvField(formatExportDate(row.startDate)) << QLatin1Char(',')
               << csvField(formatExportDate(row.endDate)) << QLatin1Char('\n');
    }

    return true;
}
