#pragma once

#include "ParseData/BusinessWorkbookTypes.h"
#include "ParseData/ExcelPreviewLoader.h"

class BusinessWorkbookResolver
{
public:
    static QString formatSheetError(const QString& sheetName, const QString& message);

    static bool workbookUsesAllSheets(const ExcelWorkbookInfo& workbookInfo);

    static bool collectWorkbookRows(const ExcelWorkbookInfo& workbookInfo,
                                    const ExcelSheetPreview& referenceSheet,
                                    int currentSheetIndex,
                                    const BusinessColumnSelection& selection,
                                    BusinessWorkbookRowsResult& result,
                                    QString& errorMessage);
};
