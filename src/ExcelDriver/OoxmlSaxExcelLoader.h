#pragma once

#include "ExcelDriver/ExcelPreviewLoader.h"

#include <QStringList>

namespace OoxmlSaxExcelLoader {

bool inspectWorkbook(const QString& filePath, QStringList& sheetNames, QString& errorMessage);

bool loadSheetPreview(const QString& filePath,
                      int sheetIndex,
                      const ExcelPreviewLimits& limits,
                      ExcelSheetPreview& sheet,
                      QString& errorMessage);

} // namespace OoxmlSaxExcelLoader
