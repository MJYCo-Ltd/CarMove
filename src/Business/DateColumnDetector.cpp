#include "Business/DateColumnDetector.h"

#include "Business/BusinessColumnUtils.h"
#include "DataParsing/DateTimeParser.h"

namespace DateColumnDetector {

namespace {

constexpr int kHeaderScanRowCount = 3;
constexpr int kMaxDataRowsToSample = 120;

int maxRowsToScanForColumnDetection(int gridRowCount)
{
    return qMin(gridRowCount, kHeaderScanRowCount + kMaxDataRowsToSample);
}

bool headerLooksLikeTime(const QString& text)
{
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty() || DateTimeParser::looksLikeDate(trimmed)) {
        return false;
    }

    return trimmed.contains(QStringLiteral("时间"))
           || trimmed.contains(QStringLiteral("日期"))
           || trimmed.contains(QStringLiteral("date"), Qt::CaseInsensitive)
           || trimmed.contains(QStringLiteral("time"), Qt::CaseInsensitive);
}

} // namespace

QList<int> DateColumnDetector::markedColumnIndices(const ExcelSheetPreview& sheet)
{
    return BusinessColumnUtils::markedColumnIndices(sheet.isDateColumn);
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

void markDateColumns(ExcelSheetPreview& sheet)
{
    sheet.isDateColumn = QVector<bool>(sheet.columnCount, false);

    const int headerRowCount = qMin(kHeaderScanRowCount, sheet.grid.size());
    for (int rowIndex = 0; rowIndex < headerRowCount; ++rowIndex) {
        const QVector<QString>& row = sheet.grid.at(rowIndex);
        for (int columnIndex = 0; columnIndex < sheet.columnCount && columnIndex < row.size(); ++columnIndex) {
            if (headerLooksLikeTime(row.at(columnIndex))) {
                sheet.isDateColumn[columnIndex] = true;
            }
        }
    }

    const int maxRowIndex = maxRowsToScanForColumnDetection(sheet.grid.size());
    for (int columnIndex = 0; columnIndex < sheet.columnCount; ++columnIndex) {
        if (sheet.isDateColumn.at(columnIndex)) {
            continue;
        }

        for (int rowIndex = 0; rowIndex < maxRowIndex; ++rowIndex) {
            const QVector<QString>& row = sheet.grid.at(rowIndex);
            if (columnIndex >= row.size()) {
                continue;
            }

            if (DateTimeParser::looksLikeDate(row.at(columnIndex))) {
                sheet.isDateColumn[columnIndex] = true;
                break;
            }
        }
    }
}

} // namespace DateColumnDetector
