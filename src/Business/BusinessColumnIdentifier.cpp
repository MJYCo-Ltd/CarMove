#include "Business/BusinessColumnIdentifier.h"

#include "Business/DateColumnDetector.h"
#include "Business/LicensePlateDetector.h"

void BusinessColumnIdentifier::identifyColumns(ExcelSheetPreview& sheet)
{
    sheet.isPlateColumn.clear();
    sheet.isDateColumn.clear();

    if (sheet.grid.isEmpty() || sheet.columnCount <= 0) {
        return;
    }

    LicensePlateDetector::markPlateColumns(sheet);
    DateColumnDetector::markDateColumns(sheet);
}

QString BusinessColumnIdentifier::formatColumnStatus(const ExcelSheetPreview& sheet)
{
    const QList<int> plateColumns = LicensePlateDetector::markedColumnIndices(sheet);
    const QList<int> dateColumns = DateColumnDetector::markedColumnIndices(sheet);

    auto formatColumnNumbers = [](const QList<int>& indices) {
        QStringList numbers;
        numbers.reserve(indices.size());
        for (int columnIndex : indices) {
            numbers.append(QString::number(columnIndex + 1));
        }
        return numbers.join(QStringLiteral("、"));
    };

    QStringList parts;
    if (plateColumns.isEmpty()) {
        parts.append(QStringLiteral("未识别到车牌列"));
    } else {
        parts.append(QStringLiteral("车牌列：%1").arg(formatColumnNumbers(plateColumns)));
    }

    if (dateColumns.isEmpty()) {
        parts.append(QStringLiteral("未识别到时间列"));
    } else {
        parts.append(QStringLiteral("时间列：%1").arg(formatColumnNumbers(dateColumns)));
    }

    return parts.join(QStringLiteral("；"));
}

void BusinessColumnIdentifier::appendColumnStatus(ExcelSheetPreview& sheet)
{
    if (sheet.grid.isEmpty()) {
        return;
    }

    const QString columnStatus = formatColumnStatus(sheet);
    if (sheet.statusMessage.isEmpty()) {
        sheet.statusMessage = columnStatus;
    } else {
        sheet.statusMessage += QStringLiteral("；") + columnStatus;
    }
}
