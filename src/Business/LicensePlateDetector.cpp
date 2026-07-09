#include "Business/LicensePlateDetector.h"
#include "ExcelDriver/ExcelPreviewColumnUtils.h"

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

bool headerLooksLikePlate(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || isChineseVehiclePlate(trimmed)) {
        return false;
    }

    return trimmed.contains(QStringLiteral("车牌"))
           || trimmed.contains(QStringLiteral("牌照"))
           || trimmed.contains(QStringLiteral("车号"))
           || trimmed.contains(QStringLiteral("plate"), Qt::CaseInsensitive);
}

void markPlateColumns(ExcelSheetPreview& sheet)
{
    sheet.isPlateColumn = QVector<bool>(sheet.columnCount, false);

    const int headerRowCount = qMin(3, sheet.grid.size());
    for (int rowIndex = 0; rowIndex < headerRowCount; ++rowIndex) {
        const QVector<QString>& row = sheet.grid.at(rowIndex);
        for (int columnIndex = 0; columnIndex < sheet.columnCount && columnIndex < row.size(); ++columnIndex) {
            if (headerLooksLikePlate(row.at(columnIndex))) {
                sheet.isPlateColumn[columnIndex] = true;
            }
        }
    }

    for (int columnIndex = 0; columnIndex < sheet.columnCount; ++columnIndex) {
        if (sheet.isPlateColumn.at(columnIndex)) {
            continue;
        }

        for (const QVector<QString>& row : sheet.grid) {
            if (columnIndex >= row.size()) {
                continue;
            }

            if (isChineseVehiclePlate(row.at(columnIndex))) {
                sheet.isPlateColumn[columnIndex] = true;
                break;
            }
        }
    }
}

} // namespace LicensePlateDetector
