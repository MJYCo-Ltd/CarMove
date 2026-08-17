#include "ExcelDriver/ExcelCellFormatter.h"
#include "DataParsing/DateTimeParser.h"

#include <QDate>
#include <QDateTime>
#include <QTime>

#include "xlsxcell.h"

QXLSX_USE_NAMESPACE

namespace {

QString formatYear(const QDate& date, int length)
{
    if (length >= 4) {
        return QString::number(date.year()).rightJustified(4, QLatin1Char('0'));
    }
    return QString::number(date.year() % 100).rightJustified(2, QLatin1Char('0'));
}

QString formatMonth(const QDate& date, int length)
{
    if (length >= 2) {
        return QString::number(date.month()).rightJustified(2, QLatin1Char('0'));
    }
    return QString::number(date.month());
}

QString formatDay(const QDate& date, int length)
{
    if (length >= 2) {
        return QString::number(date.day()).rightJustified(2, QLatin1Char('0'));
    }
    return QString::number(date.day());
}

QString formatHour(const QTime& time, int length, bool use12Hour)
{
    int hour = time.hour();
    if (use12Hour) {
        hour = hour % 12;
        if (hour == 0) {
            hour = 12;
        }
    }

    if (length >= 2) {
        return QString::number(hour).rightJustified(2, QLatin1Char('0'));
    }
    return QString::number(hour);
}

QString formatMinuteOrSecond(int value, int length)
{
    if (length >= 2) {
        return QString::number(value).rightJustified(2, QLatin1Char('0'));
    }
    return QString::number(value);
}

} // namespace

int ExcelCellFormatter::repeatCharCount(const QString& formatCode, int index, QChar ch)
{
    int count = 1;
    while (index + count < formatCode.size() && formatCode.at(index + count) == ch) {
        ++count;
    }
    return count;
}

int ExcelCellFormatter::skipBracketSection(const QString& formatCode, int index)
{
    if (index >= formatCode.size() || formatCode.at(index) != QLatin1Char('[')) {
        return index;
    }

    ++index;
    while (index < formatCode.size() && formatCode.at(index) != QLatin1Char(']')) {
        ++index;
    }
    if (index < formatCode.size()) {
        ++index;
    }
    return index;
}

QString ExcelCellFormatter::applyExcelDateFormat(const QDateTime& dateTime,
                                                 const QString& formatCode)
{
    if (!dateTime.isValid() || formatCode.isEmpty()) {
        return QString();
    }

    const QString firstSection = formatCode.section(QLatin1Char(';'), 0, 0);
    const QDate date = dateTime.date();
    const QTime time = dateTime.time();

    QString result;
    result.reserve(firstSection.size() + 16);

    for (int index = 0; index < firstSection.size();) {
        const QChar ch = firstSection.at(index);

        if (ch == QLatin1Char('[')) {
            index = skipBracketSection(firstSection, index);
            continue;
        }

        if (ch == QLatin1Char('\\')) {
            if (index + 1 < firstSection.size()) {
                result += firstSection.at(index + 1);
                index += 2;
            } else {
                ++index;
            }
            continue;
        }

        if (ch == QLatin1Char('"')) {
            ++index;
            while (index < firstSection.size() && firstSection.at(index) != QLatin1Char('"')) {
                result += firstSection.at(index);
                ++index;
            }
            if (index < firstSection.size()) {
                ++index;
            }
            continue;
        }

        if (ch == QLatin1Char('y')) {
            const int count = repeatCharCount(firstSection, index, ch);
            result += formatYear(date, count);
            index += count;
            continue;
        }

        if (ch == QLatin1Char('m')) {
            const int count = repeatCharCount(firstSection, index, ch);
            result += formatMonth(date, count);
            index += count;
            continue;
        }

        if (ch == QLatin1Char('d')) {
            const int count = repeatCharCount(firstSection, index, ch);
            result += formatDay(date, count);
            index += count;
            continue;
        }

        if (ch == QLatin1Char('h')) {
            const int count = repeatCharCount(firstSection, index, ch);
            const bool use12Hour =
                firstSection.indexOf(QLatin1String("AM/PM"), index, Qt::CaseInsensitive) >= 0
                || firstSection.indexOf(QLatin1String("A/P"), index, Qt::CaseInsensitive) >= 0;
            result += formatHour(time, count, use12Hour);
            index += count;
            continue;
        }

        if (ch == QLatin1Char('s')) {
            const int count = repeatCharCount(firstSection, index, ch);
            result += formatMinuteOrSecond(time.second(), count);
            index += count;
            continue;
        }

        if ((ch == QLatin1Char('A') || ch == QLatin1Char('a'))
            && firstSection.mid(index).startsWith(QStringLiteral("AM/PM"), Qt::CaseInsensitive)) {
            result += time.hour() < 12 ? QStringLiteral("AM") : QStringLiteral("PM");
            index += 5;
            continue;
        }

        result += ch;
        ++index;
    }

    return result;
}

QString ExcelCellFormatter::formatVariant(const QVariant& value)
{
    if (value.isNull()) {
        return QString();
    }

    if (value.typeId() == QMetaType::Double) {
        const double number = value.toDouble();
        if (qAbs(number - qRound64(number)) < 0.0000001) {
            return QString::number(static_cast<qint64>(qRound64(number)));
        }
        return QString::number(number, 'g', 15);
    }

    if (value.typeId() == QMetaType::Bool) {
        return value.toBool() ? QStringLiteral("TRUE") : QStringLiteral("FALSE");
    }

    return value.toString();
}

QString ExcelCellFormatter::formatPreviewCellValue(const QVariant& value, bool isDate1904)
{
    if (value.isNull()) {
        return QString();
    }

    if (value.typeId() == QMetaType::Double || value.typeId() == QMetaType::Int
        || value.typeId() == QMetaType::LongLong) {
        const double number = value.toDouble();
        if (number >= 20000.0 && number <= 80000.0) {
            const QDateTime dateTime = DateTimeParser::dateTimeFromExcelSerial(number, isDate1904);
            if (dateTime.isValid()) {
                if (dateTime.time() == QTime(0, 0)) {
                    return dateTime.date().toString(QStringLiteral("yyyy-MM-dd"));
                }
                return dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
            }
        }
    }

    return formatVariant(value);
}

QString ExcelCellFormatter::formatQXlsxCell(const std::shared_ptr<Cell>& cell, bool isDate1904)
{
    if (!cell) {
        return QString();
    }

    if (cell->isDateTime()) {
        const double serial = cell->value().toDouble();
        const QDateTime dateTime = DateTimeParser::dateTimeFromExcelSerial(serial, isDate1904);
        if (!dateTime.isValid()) {
            return formatVariant(cell->value());
        }

        const QString formatCode = cell->format().numberFormat();
        if (!formatCode.isEmpty()) {
            const QString formatted = applyExcelDateFormat(dateTime, formatCode);
            if (!formatted.isEmpty()) {
                return formatted;
            }
        }

        if (dateTime.time() == QTime(0, 0)) {
            return dateTime.date().toString(QStringLiteral("yyyy-MM-dd"));
        }
        return dateTime.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    }

    return formatVariant(cell->value());
}
