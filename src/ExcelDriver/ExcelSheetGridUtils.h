#ifndef EXCELSHEETGRIDUTILS_H
#define EXCELSHEETGRIDUTILS_H

#include "ExcelDriver/ExcelPreviewLoader.h"

#include <QMap>
#include <QString>
#include <QVector>

namespace ExcelSheetGridUtils {

void buildGridFromSparseCells(const QMap<int, QMap<int, QString>>& cells, ExcelSheetPreview& sheet);

void compressGridToUsedColumns(QVector<QVector<QString>>& grid, int& columnCount);

QString formatSheetStatus(int loadedRows,
                            int columnCount,
                            bool truncated,
                            const ExcelPreviewLimits& limits);

} // namespace ExcelSheetGridUtils

#endif // EXCELSHEETGRIDUTILS_H
