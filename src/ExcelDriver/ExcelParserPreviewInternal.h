#pragma once

#include "ExcelDriver/ExcelPreviewTypes.h"

#include <QString>

namespace ExcelParserPreviewInternal {

bool inspectWorkbook(const QString& filePath, ExcelWorkbookInfo& info, QString& errorMessage);

bool loadSheet(const ExcelWorkbookInfo& info,
               int sheetIndex,
               ExcelSheetPreview& sheet,
               QString& errorMessage);

} // namespace ExcelParserPreviewInternal
