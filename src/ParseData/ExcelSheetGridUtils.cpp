#include "ParseData/ExcelSheetGridUtils.h"

#include <QList>
#include <QSet>

#include <algorithm>

namespace ExcelSheetGridUtils {

void buildGridFromSparseCells(const QMap<int, QMap<int, QString>>& cells, ExcelSheetPreview& sheet)
{
    QSet<int> usedColumns;
    QList<int> usedRows;
    usedRows.reserve(cells.size());

    for (auto rowIt = cells.cbegin(); rowIt != cells.cend(); ++rowIt) {
        usedRows.append(rowIt.key());
        for (auto colIt = rowIt.value().cbegin(); colIt != rowIt.value().cend(); ++colIt) {
            usedColumns.insert(colIt.key());
        }
    }

    std::sort(usedRows.begin(), usedRows.end());

    QList<int> columns = usedColumns.values();
    std::sort(columns.begin(), columns.end());

    sheet.grid.clear();
    sheet.originalRowNumbers.clear();
    sheet.columnCount = columns.size();

    for (const int rowIndex : usedRows) {
        QVector<QString> rowData;
        rowData.reserve(columns.size());
        const QMap<int, QString>& rowCells = cells.value(rowIndex);
        for (const int columnIndex : columns) {
            rowData.append(rowCells.value(columnIndex));
        }
        sheet.grid.append(std::move(rowData));
        sheet.originalRowNumbers.append(rowIndex);
    }
}

void compressGridToUsedColumns(QVector<QVector<QString>>& grid, int& columnCount)
{
    if (grid.isEmpty()) {
        columnCount = 0;
        return;
    }

    QSet<int> usedColumns;
    for (int rowIndex = 0; rowIndex < grid.size(); ++rowIndex) {
        const QVector<QString>& row = grid.at(rowIndex);
        for (int columnIndex = 0; columnIndex < row.size(); ++columnIndex) {
            if (!row.at(columnIndex).trimmed().isEmpty()) {
                usedColumns.insert(columnIndex);
            }
        }
    }

    QList<int> columns = usedColumns.values();
    std::sort(columns.begin(), columns.end());

    if (columns.isEmpty()) {
        grid.clear();
        columnCount = 0;
        return;
    }

    QVector<QVector<QString>> compressedGrid;
    compressedGrid.reserve(grid.size());
    for (const QVector<QString>& row : grid) {
        QVector<QString> compressedRow;
        compressedRow.reserve(columns.size());
        for (const int columnIndex : columns) {
            compressedRow.append(columnIndex < row.size() ? row.at(columnIndex) : QString());
        }
        compressedGrid.append(std::move(compressedRow));
    }

    grid = std::move(compressedGrid);
    columnCount = columns.size();
}

QString formatSheetStatus(int loadedRows,
                          int columnCount,
                          bool truncated,
                          const ExcelPreviewLimits& limits)
{
    if (loadedRows <= 0 || columnCount <= 0) {
        return QStringLiteral("工作表为空");
    }
    if (truncated) {
        return QStringLiteral("大文件预览：已显示前 %1 行 × %2 列（上限 %3 行 / %4 列）")
            .arg(loadedRows)
            .arg(columnCount)
            .arg(limits.maxRows)
            .arg(limits.maxColumns);
    }
    return QStringLiteral("共 %1 行 × %2 列").arg(loadedRows).arg(columnCount);
}

} // namespace ExcelSheetGridUtils
