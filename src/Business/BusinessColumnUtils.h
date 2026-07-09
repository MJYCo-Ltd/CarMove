#ifndef BUSINESSCOLUMNUTILS_H
#define BUSINESSCOLUMNUTILS_H

#include "ExcelDriver/ExcelPreviewTypes.h"

#include <QList>
#include <QVector>

namespace BusinessColumnUtils {

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

} // namespace BusinessColumnUtils

#endif // BUSINESSCOLUMNUTILS_H
