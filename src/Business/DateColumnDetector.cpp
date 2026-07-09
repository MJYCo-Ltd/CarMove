#include "Business/DateColumnDetector.h"

#include "ExcelDriver/ExcelCellFormatter.h"
#include "ExcelDriver/ExcelPreviewColumnUtils.h"

#include <QDateTime>
#include <QRegularExpression>

namespace DateColumnDetector {

namespace {

bool isExcelDateSerialString(const QString& text, double* serialOut = nullptr)
{
    bool ok = false;
    const double serial = text.trimmed().toDouble(&ok);
    if (!ok || serial < 20000.0 || serial > 80000.0) {
        return false;
    }

    const QDateTime dateTime = ExcelCellFormatter::dateTimeFromExcelSerial(serial, false);
    if (!dateTime.isValid()) {
        return false;
    }

    if (serialOut != nullptr) {
        *serialOut = serial;
    }
    return true;
}

bool matchesDatePattern(const QString& text)
{
    static const QRegularExpression patterns[] = {
        QRegularExpression(
            QStringLiteral(R"(^\d{4}-\d{1,2}-\d{1,2}(\s+\d{1,2}:\d{1,2}(:\d{1,2})?)?$)")),
        QRegularExpression(
            QStringLiteral(R"(^\d{4}/\d{1,2}/\d{1,2}(\s+\d{1,2}:\d{1,2}(:\d{1,2})?)?$)")),
        QRegularExpression(
            QStringLiteral(R"(^\d{4}\.\d{1,2}\.\d{1,2}(\s+\d{1,2}:\d{1,2}(:\d{1,2})?)?$)")),
        QRegularExpression(QStringLiteral(R"(^\d{4}年\d{1,2}月\d{1,2}日$)")),
        QRegularExpression(
            QStringLiteral(R"(^\d{4}年\d{1,2}月\d{1,2}日\s+\d{1,2}:\d{1,2}(:\d{1,2})?$)")),
        QRegularExpression(
            QStringLiteral(R"(^\d{4}年\d{1,2}月\d{1,2}日\s+\d{1,2}时\d{1,2}分(\d{1,2}秒)?$)")),
    };

    for (const QRegularExpression& pattern : patterns) {
        if (pattern.match(text).hasMatch()) {
            return true;
        }
    }

    return false;
}

QStringList dateTimeFormatPatterns()
{
    return {
        QStringLiteral("yyyy-MM-dd hh:mm:ss"),
        QStringLiteral("yyyy/MM/dd hh:mm:ss"),
        QStringLiteral("yyyy-MM-dd hh:mm"),
        QStringLiteral("yyyy/MM/dd hh:mm"),
        QStringLiteral("yyyy-MM-dd"),
        QStringLiteral("yyyy/MM/dd"),
        QStringLiteral("yyyy/M/d hh:mm:ss"),
        QStringLiteral("yyyy/M/d hh:mm"),
        QStringLiteral("yyyy/M/d"),
        QStringLiteral("yyyy.M.d hh:mm:ss"),
        QStringLiteral("yyyy.M.d hh:mm"),
        QStringLiteral("yyyy.M.d"),
        QStringLiteral("yyyy.MM.dd hh:mm:ss"),
        QStringLiteral("yyyy.MM.dd hh:mm"),
        QStringLiteral("yyyy.MM.dd"),
        QStringLiteral("yyyy年M月d日 hh:mm:ss"),
        QStringLiteral("yyyy年M月d日 hh时mm分ss秒"),
        QStringLiteral("yyyy年M月d日"),
        QStringLiteral("yyyy年MM月dd日 hh:mm:ss"),
        QStringLiteral("yyyy年MM月dd日 hh时mm分ss秒"),
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

} // namespace

bool looksLikeDate(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return false;
    }

    if (matchesDatePattern(trimmed)) {
        return true;
    }

    if (isExcelDateSerialString(trimmed)) {
        return true;
    }

    const QStringList formats = dateTimeFormatPatterns();

    for (const QString& format : formats) {
        const QDateTime dateTime = QDateTime::fromString(trimmed, format);
        if (dateTime.isValid()) {
            return true;
        }
    }

    if (parseChineseDateText(trimmed).isValid()) {
        return true;
    }

    const QDateTime isoDateTime = QDateTime::fromString(trimmed, Qt::ISODate);
    return isoDateTime.isValid();
}

QDate parseToDate(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return QDate();
    }

    double serial = 0.0;
    if (isExcelDateSerialString(trimmed, &serial)) {
        const QDateTime dateTime = ExcelCellFormatter::dateTimeFromExcelSerial(serial, false);
        if (dateTime.isValid()) {
            return dateTime.date();
        }
    }

    const QStringList formats = dateTimeFormatPatterns();

    for (const QString& format : formats) {
        const QDateTime dateTime = QDateTime::fromString(trimmed, format);
        if (dateTime.isValid()) {
            return dateTime.date();
        }
    }

    const QDate chineseDate = parseChineseDateText(trimmed);
    if (chineseDate.isValid()) {
        return chineseDate;
    }

    const QDateTime isoDateTime = QDateTime::fromString(trimmed, Qt::ISODate);
    if (isoDateTime.isValid()) {
        return isoDateTime.date();
    }

    return QDate();
}

QList<int> DateColumnDetector::markedColumnIndices(const ExcelSheetPreview& sheet)
{
    return ExcelPreviewColumnUtils::markedColumnIndices(sheet.isDateColumn);
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
    if (trimmed.isEmpty() || looksLikeDate(trimmed)) {
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

    const int headerRowCount = qMin(3, sheet.grid.size());
    for (int rowIndex = 0; rowIndex < headerRowCount; ++rowIndex) {
        const QVector<QString>& row = sheet.grid.at(rowIndex);
        for (int columnIndex = 0; columnIndex < sheet.columnCount && columnIndex < row.size(); ++columnIndex) {
            if (headerLooksLikeTime(row.at(columnIndex))) {
                sheet.isDateColumn[columnIndex] = true;
            }
        }
    }

    for (int columnIndex = 0; columnIndex < sheet.columnCount; ++columnIndex) {
        if (sheet.isDateColumn.at(columnIndex)) {
            continue;
        }

        for (const QVector<QString>& row : sheet.grid) {
            if (columnIndex >= row.size()) {
                continue;
            }

            if (looksLikeDate(row.at(columnIndex))) {
                sheet.isDateColumn[columnIndex] = true;
                break;
            }
        }
    }
}

} // namespace DateColumnDetector
