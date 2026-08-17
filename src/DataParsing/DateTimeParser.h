#pragma once

#include <QDate>
#include <QDateTime>
#include <QString>
#include <QVariant>

namespace DateTimeParser {

QDateTime dateTimeFromExcelSerial(double serial, bool isDate1904 = false);

QDateTime parseDateTime(const QString& text);
QDateTime parseDateTime(const QVariant& value, bool isDate1904 = false);

QDate parseToDate(const QString& text);

bool looksLikeDate(const QString& text);

} // namespace DateTimeParser
