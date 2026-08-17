#include "DataParsing/DateTimeParser.h"

#include <QRegularExpression>

namespace DateTimeParser {

namespace {

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
        QStringLiteral("MM/dd/yyyy HH:mm:ss"),
        QStringLiteral("MM-dd-yyyy HH:mm:ss"),
        QStringLiteral("dd/MM/yyyy HH:mm:ss"),
        QStringLiteral("dd-MM-yyyy HH:mm:ss"),
        QStringLiteral("MM月dd日 HH:mm:ss"),
        QStringLiteral("HH:mm:ss"),
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

bool isExcelDateSerialString(const QString& text, double* serialOut = nullptr)
{
    bool ok = false;
    const double serial = text.trimmed().toDouble(&ok);
    if (!ok || serial < 20000.0 || serial > 80000.0) {
        return false;
    }

    const QDateTime dateTime = dateTimeFromExcelSerial(serial, false);
    if (!dateTime.isValid()) {
        return false;
    }

    if (serialOut != nullptr) {
        *serialOut = serial;
    }
    return true;
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
        const QDateTime dateTime = dateTimeFromExcelSerial(serial, false);
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
        const QDateTime dateTime = dateTimeFromExcelSerial(serial, false);
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

} // namespace

QDateTime dateTimeFromExcelSerial(double serial, bool isDate1904)
{
    if (serial < 0.0) {
        return QDateTime();
    }

    double adjustedSerial = serial;
    if (!isDate1904 && adjustedSerial > 60.0) {
        adjustedSerial -= 1.0;
    }

    const QDate epoch(1899, 12, 31);
    const int wholeDays = static_cast<int>(adjustedSerial);
    const double fractionalPart = adjustedSerial - static_cast<double>(wholeDays);
    const int totalSeconds = static_cast<int>(fractionalPart * 24.0 * 60.0 * 60.0 + 0.5);

    QDateTime dateTime(epoch.addDays(wholeDays).startOfDay());
    if (totalSeconds > 0) {
        dateTime = dateTime.addSecs(totalSeconds);
    }

    return dateTime;
}

QDateTime parseDateTime(const QString& text)
{
    return parseNormalizedDateTime(text);
}

QDateTime parseDateTime(const QVariant& value, bool isDate1904)
{
    if (value.isNull()) {
        return QDateTime();
    }

    if (value.typeId() == QMetaType::QDateTime) {
        return value.toDateTime();
    }

    if (value.typeId() == QMetaType::QDate) {
        return value.toDate().startOfDay();
    }

    if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Int
        || value.typeId() == QMetaType::LongLong) {
        return dateTimeFromExcelSerial(value.toDouble(), isDate1904);
    }

    return parseNormalizedDateTime(value.toString());
}

QDate parseToDate(const QString& text)
{
    const QDateTime dateTime = parseNormalizedDateTime(text);
    return dateTime.isValid() ? dateTime.date() : QDate();
}

bool looksLikeDate(const QString& text)
{
    if (!quickLooksLikeDate(text)) {
        return false;
    }
    return parseNormalizedDateTime(text).isValid();
}

} // namespace DateTimeParser
