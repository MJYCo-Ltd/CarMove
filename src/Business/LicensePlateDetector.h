#ifndef LICENSEPLATEDETECTOR_H
#define LICENSEPLATEDETECTOR_H

#include "ExcelDriver/ExcelPreviewTypes.h"

#include <QList>

namespace LicensePlateDetector {

void markPlateColumns(ExcelSheetPreview& sheet);

QList<int> markedColumnIndices(const ExcelSheetPreview& sheet);

int firstColumnIndex(const ExcelSheetPreview& sheet);

} // namespace LicensePlateDetector

#endif // LICENSEPLATEDETECTOR_H
