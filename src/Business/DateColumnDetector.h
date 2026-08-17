#ifndef DATECOLUMNDETECTOR_H
#define DATECOLUMNDETECTOR_H

#include "ExcelDriver/ExcelPreviewTypes.h"

#include <QList>

namespace DateColumnDetector {

void markDateColumns(ExcelSheetPreview& sheet);

QList<int> markedColumnIndices(const ExcelSheetPreview& sheet);

int ordinalForDataColumn(const ExcelSheetPreview& sheet, int dataColumnIndex);

int dataColumnAtOrdinal(const ExcelSheetPreview& sheet, int ordinal);

} // namespace DateColumnDetector

#endif // DATECOLUMNDETECTOR_H
