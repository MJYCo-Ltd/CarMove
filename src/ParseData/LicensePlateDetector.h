#ifndef LICENSEPLATEDETECTOR_H
#define LICENSEPLATEDETECTOR_H

#include "ParseData/ExcelPreviewLoader.h"

#include <QList>

namespace LicensePlateDetector {

bool isChineseVehiclePlate(const QString& text);

void markPlateColumns(ExcelSheetPreview& sheet);

QList<int> markedColumnIndices(const ExcelSheetPreview& sheet);

int firstColumnIndex(const ExcelSheetPreview& sheet);

} // namespace LicensePlateDetector

#endif // LICENSEPLATEDETECTOR_H
