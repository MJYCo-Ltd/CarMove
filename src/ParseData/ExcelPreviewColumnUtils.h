#ifndef EXCELPREVIEWCOLUMNUTILS_H
#define EXCELPREVIEWCOLUMNUTILS_H

#include "ParseData/ExcelPreviewLoader.h"

#include <QList>
#include <QVector>

namespace ExcelPreviewColumnUtils {

inline QList<int> markedColumnIndices(const QVector<bool>& flags)
{
    QList<int> indices;
    for (int columnIndex = 0; columnIndex < flags.size(); ++columnIndex) {
        if (flags.at(columnIndex)) {
            indices.append(columnIndex);
        }
    }
    return indices;
}

template<typename Predicate>
void markColumns(ExcelSheetPreview& sheet, QVector<bool>& flags, Predicate predicate)
{
    flags = QVector<bool>(sheet.columnCount, false);

    for (int columnIndex = 0; columnIndex < sheet.columnCount; ++columnIndex) {
        for (const QVector<QString>& row : sheet.grid) {
            if (columnIndex >= row.size()) {
                continue;
            }

            if (predicate(row.at(columnIndex))) {
                flags[columnIndex] = true;
                break;
            }
        }
    }
}

} // namespace ExcelPreviewColumnUtils

#endif // EXCELPREVIEWCOLUMNUTILS_H
