#include "Business/DateColumnDetector.h"

#include "Business/BusinessExcelDateUtils.h"
#include "Business/BusinessColumnUtils.h"

#include <QDateTime>
#include <QRegularExpression>

namespace DateColumnDetector {

namespace {

constexpr int kHeaderScanRowCount = 3;
constexpr int kMaxDataRowsToSample = 120;

bool isExcelDateSerialString(const QString& text, double* serialOut = nullptr)
{
    bool ok = false;
    const double serial = text.trimmed().toDouble(&ok);
    if (!ok || serial < 20000.0 || serial > 80000.0) {
        return false;
    }

    const QDateTime dateTime = BusinessExcelDateUtils::dateTimeFromExcelSerial(serial, false);
    if (!dateTime.isValid()) {
        return false;
    }

    if (serialOut != nullptr) {
        *serialOut = serial;
    }
    return true;
}

QString normalizeDateTimeText(const QString& text)
{
    QString normalized = text.trimmed();
    normalized.replace(QChar(0x00A0), QLatin1Char(' '));
    normalized.replace(QChar(0x3000), QLatin1Char(' '));
    while (normalized.contains(QStringLiteral("  "))) {
        normalized.replace(QStringLiteral("  "), QStringLiteral(" "));
    }
    if (normalized.contains(QLatin1Char('T'))) {
        normalized.replace(QLatin1Char('T'), QLatin1Char(' '));
    }
    if (normalized.contains(QLatin1Char(':'))) {
        static const QRegularExpression trailingFraction(QStringLiteral(R"(\.\d+$)"));
        normalized.remove(trailingFraction);
    }
    return normalized.trimmed();
}

QStringList dateTimeFormatPatterns()
{
    return {
        QStringLiteral("yyyy-MM-dd HH:mm:ss"),
        QStringLiteral("yyyy/MM/dd HH:mm:ss"),
        QStringLiteral("yyyy-MM-dd HH:mm"),
        QStringLiteral("yyyy/MM/dd HH:mm"),
        QStringLiteral("yyyy-MM-dd"),
        QStringLiteral("yyyy/MM/dd"),
        QStringLiteral("yyyy-M-d HH:mm:ss"),
        QStringLiteral("yyyy-M-d HH:mm"),
        QStringLiteral("yyyy-M-d"),
        QStringLiteral("yyyy/M/d HH:mm:ss"),
        QStringLiteral("yyyy/M/d HH:mm"),
        QStringLiteral("yyyy/M/d"),
        QStringLiteral("yyyy.M.d HH:mm:ss"),
        QStringLiteral("yyyy.M.d HH:mm"),
        QStringLiteral("yyyy.M.d"),
        QStringLiteral("yyyy.MM.dd HH:mm:ss"),
        QStringLiteral("yyyy.MM.dd HH:mm"),
        QStringLiteral("yyyy.MM.dd"),
        QStringLiteral("yyyy年M月d日 HH:mm:ss"),
        QStringLiteral("yyyy年M月d日 HH时mm分ss秒"),
        QStringLiteral("yyyy年M月d日"),
        QStringLiteral("yyyy年MM月dd日 HH:mm:ss"),
        QStringLiteral("yyyy年MM月dd日 HH时mm分ss秒"),
        QStringLiteral("yyyy年MM月dd日"),
    };
}

QDate parseChineseDateText(const QString& text)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^(\d{4})年(\d{1,2})月(\d{1,2})日)"));
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    if (!match.hasMatch()) {
        return QDate();
    }

    const QDate date(match.captured(1).toInt(),
                     match.captured(2).toInt(),
                     match.captured(3).toInt());
    return date.isValid() ? date : QDate();
}

bool quickLooksLikeDate(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || trimmed.size() < 6) {
        return false;
    }

    static const QRegularExpression hasDigit(QStringLiteral(R"(\d)"));
    if (!hasDigit.match(trimmed).hasMatch()) {
        return false;
    }

    double serial = 0.0;
    if (isExcelDateSerialString(trimmed, &serial)) {
        const QDateTime dateTime = BusinessExcelDateUtils::dateTimeFromExcelSerial(serial, false);
        const int year = dateTime.date().year();
        if (year >= 1990 && year <= 2035) {
            return true;
        }
    }

    static const QRegularExpression quickPatterns[] = {
        QRegularExpression(
            QStringLiteral(R"(^\d{4}-\d{1,2}-\d{1,2}([T\s]+\d{1,2}:\d{1,2}(:\d{1,2})?(\.\d+)?)?$)")),
        QRegularExpression(
            QStringLiteral(R"(^\d{4}/\d{1,2}/\d{1,2}([T\s]+\d{1,2}:\d{1,2}(:\d{1,2})?(\.\d+)?)?$)")),
        QRegularExpression(
            QStringLiteral(R"(^\d{4}\.\d{1,2}\.\d{1,2}([T\s]+\d{1,2}:\d{1,2}(:\d{1,2})?(\.\d+)?)?$)")),
        QRegularExpression(QStringLiteral(R"(^\d{4}年\d{1,2}月\d{1,2}日$)")),
        QRegularExpression(
            QStringLiteral(R"(^\d{4}年\d{1,2}月\d{1,2}日\s+\d{1,2}:\d{1,2}(:\d{1,2})?(\.\d+)?$)")),
        QRegularExpression(
            QStringLiteral(R"(^\d{4}年\d{1,2}月\d{1,2}日\s+\d{1,2}时\d{1,2}分(\d{1,2}秒)?$)")),
    };

    for (const QRegularExpression& pattern : quickPatterns) {
        if (pattern.match(trimmed).hasMatch()) {
            return true;
        }
    }

    return false;
}

QDateTime parseNormalizedDateTime(const QString& text)
{
    const QString normalized = normalizeDateTimeText(text);
    if (normalized.isEmpty()) {
        return QDateTime();
    }

    double serial = 0.0;
    if (isExcelDateSerialString(normalized, &serial)) {
        const QDateTime dateTime = BusinessExcelDateUtils::dateTimeFromExcelSerial(serial, false);
        if (dateTime.isValid()) {
            return dateTime;
        }
    }

    for (const QString& format : dateTimeFormatPatterns()) {
        const QDateTime dateTime = QDateTime::fromString(normalized, format);
        if (dateTime.isValid()) {
            return dateTime;
        }
    }

    const QDate chineseDate = parseChineseDateText(normalized);
    if (chineseDate.isValid()) {
        return QDateTime(chineseDate.startOfDay());
    }

    QDateTime isoDateTime = QDateTime::fromString(normalized, Qt::ISODate);
    if (isoDateTime.isValid()) {
        return isoDateTime;
    }

    isoDateTime = QDateTime::fromString(normalized, Qt::ISODateWithMs);
    return isoDateTime;
}

int maxRowsToScanForColumnDetection(int gridRowCount)
{
    return qMin(gridRowCount, kHeaderScanRowCount + kMaxDataRowsToSample);
}

} // namespace

bool looksLikeDate(const QString& text)
{
    if (!quickLooksLikeDate(text)) {
        return false;
    }
    return parseNormalizedDateTime(text).isValid();
}

QDate parseToDate(const QString& text)
{
    const QDateTime dateTime = parseNormalizedDateTime(text);
    return dateTime.isValid() ? dateTime.date() : QDate();
}

QList<int> DateColumnDetector::markedColumnIndices(const ExcelSheetPreview& sheet)
{
    return BusinessColumnUtils::markedColumnIndices(sheet.isDateColumn);
}

int DateColumnDetector::ordinalForDataColumn(const ExcelSheetPreview& sheet, int dataColumnIndex)
{
    return markedColumnIndices(sheet).indexOf(dataColumnIndex);
}

int DateColumnDetector::dataColumnAtOrdinal(const ExcelSheetPreview& sheet, int ordinal)
{
    const QList<int> indices = markedColumnIndices(sheet);
    if (ordinal < 0 || ordinal >= indices.size()) {
        return -1;
    }
    return indices.at(ordinal);
}

bool headerLooksLikeTime(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || quickLooksLikeDate(trimmed)) {
        return false;
    }

    return trimmed.contains(QStringLiteral("时间"))
           || trimmed.contains(QStringLiteral("日期"))
           || trimmed.contains(QStringLiteral("date"), Qt::CaseInsensitive)
           || trimmed.contains(QStringLiteral("time"), Qt::CaseInsensitive);
}

void markDateColumns(ExcelSheetPreview& sheet)
{
    sheet.isDateColumn = QVector<bool>(sheet.columnCount, false);

    const int headerRowCount = qMin(kHeaderScanRowCount, sheet.grid.size());
    for (int rowIndex = 0; rowIndex < headerRowCount; ++rowIndex) {
        const QVector<QString>& row = sheet.grid.at(rowIndex);
        for (int columnIndex = 0; columnIndex < sheet.columnCount && columnIndex < row.size(); ++columnIndex) {
            if (headerLooksLikeTime(row.at(columnIndex))) {
                sheet.isDateColumn[columnIndex] = true;
            }
        }
    }

    const int maxRowIndex = maxRowsToScanForColumnDetection(sheet.grid.size());
    for (int columnIndex = 0; columnIndex < sheet.columnCount; ++columnIndex) {
        if (sheet.isDateColumn.at(columnIndex)) {
            continue;
        }

        for (int rowIndex = 0; rowIndex < maxRowIndex; ++rowIndex) {
            const QVector<QString>& row = sheet.grid.at(rowIndex);
            if (columnIndex >= row.size()) {
                continue;
            }

            if (quickLooksLikeDate(row.at(columnIndex))) {
                sheet.isDateColumn[columnIndex] = true;
                break;
            }
        }
    }
}

} // namespace DateColumnDetector
