#include "ParseData/LicensePlateDetector.h"
#include "ParseData/ExcelPreviewColumnUtils.h"

#include <QRegularExpression>

namespace LicensePlateDetector {

namespace {

QRegularExpression standardPlateRegex()
{
    static const QRegularExpression regex(
        QStringLiteral("^[京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼使领]"
                       "[A-HJ-NP-Z][A-HJ-NP-Z0-9]{4}[A-HJ-NP-Z0-9挂学警港澳]?$"));
    return regex;
}

QRegularExpression newEnergyPlateRegex()
{
    static const QRegularExpression regex(
        QStringLiteral("^[京津沪渝冀豫云辽黑湘皖鲁新苏浙赣鄂桂甘晋蒙陕吉闽贵粤青藏川宁琼使领]"
                       "[A-HJ-NP-Z][A-HJ-NP-Z0-9]{5}[DF]$"));
    return regex;
}

QString normalizePlateText(const QString& text)
{
    QString normalized = text.trimmed();
    normalized.remove(QRegularExpression(QStringLiteral("\\s+")));
    return normalized.toUpper();
}

} // namespace

bool isChineseVehiclePlate(const QString& text)
{
    const QString normalized = normalizePlateText(text);
    if (normalized.size() < 7 || normalized.size() > 8) {
        return false;
    }

    return standardPlateRegex().match(normalized).hasMatch()
           || newEnergyPlateRegex().match(normalized).hasMatch();
}

QList<int> markedColumnIndices(const ExcelSheetPreview& sheet)
{
    return ExcelPreviewColumnUtils::markedColumnIndices(sheet.isPlateColumn);
}

int firstColumnIndex(const ExcelSheetPreview& sheet)
{
    const QList<int> indices = markedColumnIndices(sheet);
    return indices.isEmpty() ? -1 : indices.first();
}

void markPlateColumns(ExcelSheetPreview& sheet)
{
    ExcelPreviewColumnUtils::markColumns(
        sheet, sheet.isPlateColumn, [](const QString& text) {
            return isChineseVehiclePlate(text);
        });
}

} // namespace LicensePlateDetector
