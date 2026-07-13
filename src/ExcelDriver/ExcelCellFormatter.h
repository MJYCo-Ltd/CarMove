#pragma once

#include <QString>
#include <QVariant>
#include <memory>

namespace QXlsx {
class Cell;
}

class ExcelCellFormatter
{
public:
    static QString formatQXlsxCell(const std::shared_ptr<QXlsx::Cell>& cell, bool isDate1904);
    static QString formatVariant(const QVariant& value);
    static QString formatPreviewCellValue(const QVariant& value, bool isDate1904 = false);
    static QDateTime dateTimeFromExcelSerial(double serial, bool isDate1904 = false);

private:
    static QString applyExcelDateFormat(const QDateTime& dateTime, const QString& formatCode);
    static int repeatCharCount(const QString& formatCode, int index, QChar ch);
    static int skipBracketSection(const QString& formatCode, int index);
};
