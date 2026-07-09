#pragma once

#include "Business/BusinessWorkbookTypes.h"
#include "ExcelDriver/ExcelPreviewTypes.h"

class ExcelParserManager;

class BusinessWorkbookResolver
{
public:
    static QString formatSheetError(const QString& sheetName, const QString& message);

    static bool workbookUsesAllSheets(const ExcelWorkbookInfo& workbookInfo);

    static bool collectWorkbookRows(const ExcelWorkbookInfo& workbookInfo,
                                    const ExcelSheetPreview& referenceSheet,
                                    int currentSheetIndex,
                                    const BusinessColumnSelection& selection,
                                    ExcelParserManager& parser,
                                    BusinessWorkbookRowsResult& result,
                                    QString& errorMessage);

    /// 导出前对单表业务行排序并按「车牌+开始+结束」去重
    static void prepareRowsForExport(QList<BusinessExportRow>& rows);

    /// 批量截图等场景：按 sheet 索引逐表收集业务行（不去重、不排序）
    static bool collectSheetBusinessRows(const ExcelWorkbookInfo& workbookInfo,
                                         const ExcelSheetPreview& referenceSheet,
                                         int targetSheetIndex,
                                         const BusinessColumnSelection& selection,
                                         ExcelParserManager& parser,
                                         BusinessSheetRows& outSheetRows,
                                         QString& errorMessage,
                                         bool* sheetSkipped = nullptr);

    static QList<int> processableSheetIndices(const ExcelWorkbookInfo& workbookInfo,
                                              int currentSheetIndex);
};
