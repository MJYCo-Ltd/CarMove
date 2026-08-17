#include "Business/LicensePlateDetector.h"

#include "Business/BusinessColumnUtils.h"
#include "DataParsing/LicensePlate.h"

namespace LicensePlateDetector {

QList<int> markedColumnIndices(const ExcelSheetPreview& sheet)
{
    return BusinessColumnUtils::markedColumnIndices(sheet.isPlateColumn);
}

int firstColumnIndex(const ExcelSheetPreview& sheet)
{
    const QList<int> indices = markedColumnIndices(sheet);
    return indices.isEmpty() ? -1 : indices.first();
}

bool headerLooksLikePlate(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || LicensePlate::isChineseVehiclePlate(trimmed)) {
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

    const int maxRowIndex = qMin(sheet.grid.size(), headerRowCount + 120);
    for (int columnIndex = 0; columnIndex < sheet.columnCount; ++columnIndex) {
        if (sheet.isPlateColumn.at(columnIndex)) {
            continue;
        }

        for (int rowIndex = 0; rowIndex < maxRowIndex; ++rowIndex) {
            const QVector<QString>& row = sheet.grid.at(rowIndex);
            if (columnIndex >= row.size()) {
                continue;
            }

            if (LicensePlate::isChineseVehiclePlate(row.at(columnIndex))) {
                sheet.isPlateColumn[columnIndex] = true;
                break;
            }
        }
    }
}

} // namespace LicensePlateDetector
