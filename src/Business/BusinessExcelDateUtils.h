#pragma once

#include <QDateTime>

namespace BusinessExcelDateUtils {

inline QDateTime dateTimeFromExcelSerial(double serial, bool isDate1904 = false)
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

} // namespace BusinessExcelDateUtils
